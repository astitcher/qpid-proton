/*
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
 */

#include <proton/connection_driver.h>
#include <proton/connection.h>
#include <proton/session.h>
#include <proton/link.h>
#include <proton/delivery.h>
#include <proton/message.h>

#include <proton/codec.h>
#include <proton/transport.h>

#include <stdlib.h>
#include <string.h>

#define PN_BYTES_LITERAL(X) (pn_bytes(sizeof(#X)-1, #X))

struct event_handler_t;
typedef pn_event_type_t (*event_handler_fn)(struct event_handler_t*, pn_event_t*);

/* A handler that logs event types and delegates to another handler */
typedef struct event_handler_t {
  event_handler_fn f;
  /* Specific context slots for proton objects commonly used by handlers  */
} event_handler_t;

void event_handler_init(event_handler_t *th, event_handler_fn f) {
  th->f = f;
}

/* A pn_connection_driver_t with a test_handler */
typedef struct test_connection_driver_t {
  event_handler_t handler;
  pn_connection_driver_t driver;
} test_connection_driver_t;

void test_connection_driver_init(test_connection_driver_t *d, event_handler_fn f)
{
  event_handler_init(&d->handler, f);
  pn_connection_driver_init(&d->driver, NULL, NULL);
}

void test_connection_driver_destroy(test_connection_driver_t *d) {
  pn_connection_driver_destroy(&d->driver);
}

pn_event_type_t test_connection_driver_handle(test_connection_driver_t *d) {
  for (pn_event_t *e = pn_connection_driver_next_event(&d->driver);
       e;
       e = pn_connection_driver_next_event(&d->driver))
  {
    pn_event_type_t et = d->handler.f ? d->handler.f(&d->handler, e) : PN_EVENT_NONE;
    if (et) return et;
  }
  return PN_EVENT_NONE;
}

/* Transfer data from one driver to another in memory */
static int test_connection_drivers_xfer(test_connection_driver_t *dst, test_connection_driver_t *src)
{
  pn_bytes_t wb = pn_connection_driver_write_buffer(&src->driver);
  pn_rwbytes_t rb =  pn_connection_driver_read_buffer(&dst->driver);
  size_t size = rb.size < wb.size ? rb.size : wb.size;
  if (size) {
    memcpy(rb.start, wb.start, size);
    pn_connection_driver_write_done(&src->driver, size);
    pn_connection_driver_read_done(&dst->driver, size);
  }
  return size;
}

/* Run a pair of test drivers till there is nothing to do or one of their handlers returns non-0
   In that case return that driver
*/
test_connection_driver_t* test_connection_drivers_run(test_connection_driver_t *a, test_connection_driver_t *b)
{
  int data = 0;
  do {
    if (test_connection_driver_handle(a)) return a;
    if (test_connection_driver_handle(b)) return b;
    data = test_connection_drivers_xfer(a, b) + test_connection_drivers_xfer(b, a);
  } while (data || pn_connection_driver_has_event(&a->driver) || pn_connection_driver_has_event(&b->driver));
  return NULL;
}

/* A fixed size buffer for encoding and decoding*/
#define BUFFSIZE 1024
static char buf[BUFFSIZE] = { 0 };

static int msgs = 0;
static int acks = 0;
static int msg_limit = 1000;

/* Handler that replies to REMOTE_OPEN, stores the opened object on the handler */
static pn_event_type_t client_handler(event_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_REMOTE_OPEN:
    pn_connection_open(pn_event_connection(e));
    break;
   case PN_SESSION_REMOTE_OPEN:
    pn_session_open(pn_event_session(e));
    break;
   case PN_LINK_REMOTE_OPEN:
    pn_link_open(pn_event_link(e));
    break;
   case PN_LINK_FLOW: {
    if (msgs>=msg_limit) break;

    /* Encode and send a message */
    pn_link_t *link = pn_event_link(e);

    pn_message_t *m = pn_message();
    pn_data_put_string(pn_message_body(m), pn_bytes(4, "abc")); /* Include trailing NULL */

    size_t size = sizeof(buf);
    pn_message_encode(m, buf, &size);
    pn_message_free(m);

    pn_delivery(link, PN_BYTES_LITERAL(x));
    pn_link_send(link, &buf[0], size);
    pn_link_advance(link);

    msgs++;
    break;
   }
   case PN_DELIVERY:
    /* Close after sending 10 messages */
    acks++;
    if (acks>=msg_limit) pn_connection_close(pn_event_connection(e));
    break;
   default:
    break;
  }
  return PN_EVENT_NONE;
}

/* Handler that returns control on PN_DELIVERY and stores the delivery on the handler */
static pn_event_type_t server_handler(event_handler_t *th, pn_event_t *e) {
  switch (pn_event_type(e)) {
   case PN_CONNECTION_REMOTE_OPEN:
    pn_connection_open(pn_event_connection(e));
    break;
   case PN_SESSION_REMOTE_OPEN:
    pn_session_open(pn_event_session(e));
    break;
   case PN_LINK_REMOTE_OPEN: {
    pn_link_t *link = pn_event_link(e);
    pn_link_open(link);
    pn_link_flow(link, 1);
    break;
   }
   case PN_DELIVERY: {
    /* Receive and decode the message */
    pn_delivery_t *dlv = pn_event_delivery(e);
    pn_link_t *link = pn_delivery_link(dlv);

    size_t size = pn_delivery_pending(dlv);
    pn_link_recv(link, &buf[0], size);

    pn_message_t *m = pn_message();
    pn_message_clear(m);
    pn_message_decode(m, &buf[0], size);
    pn_data_t *body = pn_message_body(m);
    pn_data_rewind(body);
    pn_message_free(m);

    /* Acknowledge delivery */
    pn_delivery_update(dlv, PN_ACCEPTED);
    pn_delivery_settle(dlv);

    /* Allow another message */
    pn_link_flow(link, 1);
    break;
   }
   default:
    break;
  }
  return PN_EVENT_NONE;
}

static void message_transfer(void) {
  test_connection_driver_t client, server;
  test_connection_driver_init(&client, client_handler);
  test_connection_driver_init(&server, server_handler);
  pn_transport_set_server(server.driver.transport);

  pn_connection_open(client.driver.connection);
  pn_session_t *ssn = pn_session(client.driver.connection);
  pn_session_open(ssn);
  pn_link_open(pn_sender(ssn, "x"));

  test_connection_drivers_run(&client, &server);

  test_connection_driver_destroy(&client);
  test_connection_driver_destroy(&server);
}

int main(int argc, char **argv) {
    if (argc > 1) msg_limit = atoi(argv[1]);
    message_transfer();
}
