/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

/* This is currently epoll implementation specific - and will need changing for the other proactors */

#include "epoll-internal.h"
#include "proactor-internal.h"
#include "raw_connection-internal.h"

#include <proton/proactor.h>
#include <proton/listener.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/epoll.h>

/* epoll specific raw connection struct */
struct praw_connection_t {
  psocket_t psocket;
  pcontext_t context;
  struct pn_netaddr_t local, remote; /* Actual addresses */
  pmutex rearm_mutex;                /* protects pconnection_rearm from out of order arming*/
  pn_event_batch_t batch;
  struct pn_raw_connection_t *raw_connection;
  struct addrinfo *addrinfo;         /* Resolved address list */
  struct addrinfo *ai;               /* Current connect address */
  const char *host, *port;
  uint32_t current_arm;  // active epoll io events
  bool connected;
  bool disconnected;
  char addr_buf[1];
};

static void psocket_error(praw_connection_t *rc, int err, const char* what) {
  strerrorbuf msg;
  pstrerror(err, msg);
  pni_proactor_set_cond(rc->raw_connection->condition, what, rc->host, rc->port, msg);
}

static void psocket_gai_error(praw_connection_t *rc, int gai_err, const char* what) {
  pni_proactor_set_cond(rc->raw_connection->condition, what, rc->host, rc->port, gai_strerror(gai_err));
}

static void praw_connection_connected_lh(praw_connection_t *prc) {
  prc->connected = true;
  if (prc->addrinfo) {
    freeaddrinfo(prc->addrinfo);
        prc->addrinfo = NULL;
  }
  prc->ai = NULL;
  socklen_t len = sizeof(prc->remote.ss);
  (void)getpeername(prc->psocket.epoll_io.fd, (struct sockaddr*)&prc->remote.ss, &len);

  pni_raw_connected(prc->raw_connection);
}

/* multi-address connections may call pconnection_start multiple times with diffferent FDs  */
static void praw_connection_start(praw_connection_t *prc, int fd) {
  int efd = prc->psocket.proactor->epollfd;

  /* Get the local socket name now, get the peer name in pconnection_connected */
  socklen_t len = sizeof(prc->local.ss);
  (void)getsockname(fd, (struct sockaddr*)&prc->local.ss, &len);

  epoll_extended_t *ee = &prc->psocket.epoll_io;
  if (ee->polling) {     /* This is not the first attempt, stop polling and close the old FD */
    int fd = ee->fd;     /* Save fd, it will be set to -1 by stop_polling */
    stop_polling(ee, efd);
    pclosefd(prc->psocket.proactor, fd);
  }
  ee->fd = fd;
  prc->current_arm = ee->wanted = EPOLLIN | EPOLLOUT;
  start_polling(ee, efd);  // TODO: check for error
}

/* Called on initial connect, and if connection fails to try another address */
static void praw_connection_maybe_connect_lh(praw_connection_t *prc) {
  errno = 0;
  if (!prc->connected) {         /* Not yet connected */
    while (prc->ai) {            /* Have an address */
      struct addrinfo *ai = prc->ai;
            prc->ai = prc->ai->ai_next; /* Move to next address in case this fails */
      int fd = socket(ai->ai_family, SOCK_STREAM, 0);
      if (fd >= 0) {
        configure_socket(fd);
        if (!connect(fd, ai->ai_addr, ai->ai_addrlen) || errno == EINPROGRESS) {
                  praw_connection_start(prc, fd);
          return;               /* Async connection started */
        } else {
          close(fd);
        }
      }
      /* connect failed immediately, go round the loop to try the next addr */
    }
    freeaddrinfo(prc->addrinfo);
        prc->addrinfo = NULL;
    /* If there was a previous attempted connection, let the poller discover the
     *       errno from its socket, otherwise set the current error. */
    if (prc->psocket.epoll_io.fd < 0) {
      psocket_error(prc, errno ? errno : ENOTCONN, "on connect");
    }
  }
    prc->disconnected = true;
}

//
// Raw socket API
//
static pn_event_t * pni_raw_batch_next(pn_event_batch_t *batch);

static void praw_connection_init(praw_connection_t *prc, pn_proactor_t *p, pn_raw_connection_t *rc, const char *addr, size_t addrlen) {
  memset(prc, 0, sizeof(*prc));

  pcontext_init(&prc->context, RAW_CONNECTION, p);
  psocket_init(&prc->psocket, p, RAW_CONNECTION_IO);
  pni_parse_addr(addr, prc->addr_buf, addrlen+1, &prc->host, &prc->port);

  prc->current_arm = 0;
  prc->connected = false;
  prc->disconnected = false;
  prc->batch.next_event = pni_raw_batch_next;

  pmutex_init(&prc->rearm_mutex);

  prc->raw_connection = rc;
  rc->freeable = false;
}

static void praw_connection_cleanup(praw_connection_t *prc) {
  //assert(pconnection_is_final(prc));
  int fd = prc->psocket.epoll_io.fd;
  stop_polling(&prc->psocket.epoll_io, prc->psocket.proactor->epollfd);
  if (fd != -1)
    pclosefd(prc->psocket.proactor, fd);

  lock(&prc->context.mutex);
  bool can_free = proactor_remove(&prc->context);
  unlock(&prc->context.mutex);
  if (can_free) {
    prc->raw_connection->freeable = true;
    free(prc);
  }
  // else proactor_disconnect logic owns prc and its final free
}

void pn_proactor_raw_connect(pn_proactor_t *p, pn_raw_connection_t *rc, const char *addr) {
  size_t addrlen = strlen(addr);
  praw_connection_t *prc = (praw_connection_t*) malloc(sizeof(praw_connection_t)+addrlen);
  assert(prc); // TODO: memory safety
  praw_connection_init(prc, p, rc, addr, addrlen);
  // TODO: check case of proactor shutting down

  lock(&prc->context.mutex);
  proactor_add(&prc->context);

  bool notify = false;
  bool notify_proactor = false;

  if (prc->disconnected) {
    notify = wake(&prc->context);    /* Error during initialization */
  } else {
    int gai_error = pgetaddrinfo(prc->host, prc->port, 0, &prc->addrinfo);
    if (!gai_error) {
      prc->ai = prc->addrinfo;
      praw_connection_maybe_connect_lh(prc); /* Start connection attempts */
      if (prc->disconnected) notify = wake(&prc->context);
    } else {
      psocket_gai_error(prc, gai_error, "connect to ");
      notify = wake(&prc->context);
      lock(&p->context.mutex);
      notify_proactor = wake_if_inactive(p);
      unlock(&p->context.mutex);
    }
  }
  /* We need to issue INACTIVE on immediate failure */
  unlock(&prc->context.mutex);
  if (notify) wake_notify(&prc->context);
  if (notify_proactor) wake_notify(&p->context);
}

void pn_listener_raw_accept(pn_listener_t *l, pn_raw_connection_t *rc) {
  praw_connection_t *prc = (praw_connection_t*) malloc(sizeof(praw_connection_t));
  assert(prc); // TODO: memory safety
  praw_connection_init(prc, pn_listener_proactor(l), rc, "", 0);
  // TODO: fuller sanity check on input args

  int err = 0;
  int fd = -1;
  bool notify = false;
  lock(&l->context.mutex);
  if (l->context.closing)
    err = EBADF;
  else {
    accepted_t *a = listener_accepted_next(l);
    if (a) {
      fd = a->accepted_fd;
      a->accepted_fd = -1;
    }
    else err = EWOULDBLOCK;
  }

  proactor_add(&prc->context);
  lock(&prc->context.mutex);
  if (fd >= 0) {
    configure_socket(fd);
      praw_connection_start(prc, fd);
      praw_connection_connected_lh(prc);
  }
  else
    psocket_error(prc, err, "pn_listener_accept");
  if (!l->context.working && listener_has_event(l))
    notify = wake(&l->context);
  unlock(&prc->context.mutex);
  unlock(&l->context.mutex);
  if (notify) wake_notify(&l->context);
}

static pn_event_t *pni_raw_batch_next(pn_event_batch_t *batch) {
  pn_raw_connection_t *raw = containerof(batch, praw_connection_t, batch)->raw_connection;
  return pni_raw_event_next(raw);
}

pcontext_t *pni_psocket_raw_context(psocket_t* ps) {
  return &containerof(ps, praw_connection_t, psocket)->context;
}

praw_connection_t *pni_batch_raw_connection(pn_event_batch_t *batch) {
  return (batch->next_event == pni_raw_batch_next) ?
    containerof(batch, praw_connection_t, batch) : NULL;
}

pcontext_t *pni_raw_connection_context(praw_connection_t *rc) {
  return &rc->context;
}

static long snd(int fd, const void* b, size_t s) {
  return send(fd, b, s, MSG_NOSIGNAL | MSG_DONTWAIT);
}

static long rcv(int fd, void* b, size_t s) {
  return recv(fd, b, s, MSG_DONTWAIT);
}

static void  set_error(pn_raw_connection_t *conn, const char *msg, int err) {
  char what[100];
  strerror_r(err, what, sizeof(what));
  pni_proactor_set_cond(conn->condition, what, NULL, NULL, msg);
}


pn_event_batch_t *pni_raw_connection_process(pcontext_t *c, bool sched_wake) {
  praw_connection_t *raw = containerof(c, praw_connection_t, context);
  int events = raw->psocket.sched_io_events;
  int fd = raw->psocket.epoll_io.fd;
  if (!raw->connected) {
      praw_connection_connected_lh(raw);
  }
  if (events & EPOLLIN) pni_raw_read(raw->raw_connection, fd, rcv, set_error);
  if (events & EPOLLOUT) pni_raw_write(raw->raw_connection, fd, snd, set_error);
  return &raw->batch;
}

void pni_raw_connection_done(praw_connection_t *rc) {
  pn_proactor_t *p = rc->context.proactor;
  tslot_t *ts = rc->context.runner;
  pn_raw_connection_t *raw = rc->raw_connection;
  int wanted =
    (pni_raw_can_read(raw)  ? EPOLLIN : 0) |
    (pni_raw_can_write(raw) ? EPOLLOUT : 0);
  if (wanted) {
    rc->current_arm = rc->psocket.epoll_io.wanted = wanted;
    rearm_polling(&rc->psocket.epoll_io, p->epollfd);  // TODO: check for error
  } else {
    bool finished_disconnect = raw->rclosed && raw->wclosed && !raw->disconnectpending;
    if (finished_disconnect) {
      // If we're closed and we've sent the disconnect then close
      praw_connection_cleanup(rc);
    }
    if (raw->freed) {
      if (!finished_disconnect) {
        rc->raw_connection = NULL;
      }
      pni_raw_finalize(raw);
    }
  }
  lock(&p->sched_mutex);
  bool notify = unassign_thread(ts, UNUSED);
  unlock(&p->sched_mutex);
  if (notify) wake_notify(&p->context);
}
