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

#include <proton/raw_connection.h>
#include "proactor/proactor-internal.h"

#include "pn_test.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <string>
#include <vector>

using namespace pn_test;
using Catch::Matchers::Contains;
using Catch::Matchers::Equals;

namespace {
  int read_err;
  void set_read_error(pn_raw_connection_t*, const char*, int err) {
    read_err = err;
  }
  int write_err;
  void set_write_error(pn_raw_connection_t*, const char*, int err) {
    write_err = err;
  }
#ifdef MSG_DONTWAIT
  long rcv(int fd, void* b, size_t s) {
    read_err = 0;
    return ::recv(fd, b, s, MSG_DONTWAIT);
  }

  void freepair(int fds[2]) {
      ::close(fds[0]);
      ::close(fds[1]);
  }

  void rcv_stop(int fd) {
      ::shutdown(fd, SHUT_RD);
  }

  void snd_stop(int fd) {
      ::shutdown(fd, SHUT_WR);
  }

#ifdef MSG_NOSIGNAL
  long snd(int fd, const void* b, size_t s) {
    write_err = 0;
    return ::send(fd, b, s, MSG_NOSIGNAL | MSG_DONTWAIT);
  }

  int makepair(int fds[2]) {
    return ::socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, fds);
  }
#elif defined(SO_NOSIGPIPE)
  long snd(int fd, const void* b, size_t s) {
    write_err = 0;
    return ::send(fd, b, s, MSG_DONTWAIT);
  }

  int makepair(int fds[2]) {
    int rc = ::socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, fds);
    if (rc == 0) {
      int optval = 1;
      ::setsockopt(fds[0], SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval));
      ::setsockopt(fds[1], SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval));
    }
    return rc;
  }
#endif
#else
// Dummy functions for compile under windows, but test will fail
  long rcv(int fd, void*, size_t){
    read_err = 0;
    return 0;
  }

  long snd(int fd, const void*, size_t){
    write_err = 0;
    return 0;
  }

  void rcv_stop(int fd) {
  }

  void snd_stop(int fd) {
  }

  int makepair(int fds[2]) {
    return -1;
  }

  void freepair(int fds[2]) {
  }
#endif

  // Block of memory for buffers
  const size_t BUFFMEMSIZE = 8*1024;
  const size_t RBUFFCOUNT = 32;
  const size_t WBUFFCOUNT = 32;

  char rbuffer_memory[BUFFMEMSIZE];
  char *rbuffer_brk = rbuffer_memory;

  pn_raw_buffer_t rbuffs[RBUFFCOUNT];
  pn_raw_buffer_t wbuffs[WBUFFCOUNT];

  class ReadBuffer {};
  class WriteBuffer {};

  class BufferAllocator {
    char* buffer;
    uint32_t size;
    uint32_t brk;

  public:
    BufferAllocator(char* b, uint32_t s) : buffer(b), size(s), brk(0) {};

    char* next(uint32_t s) {
      if ( brk+s > size) return NULL;

      char *r = buffer+brk;
      brk += s;
      return r;
    }

    template <class B>
    pn_raw_buffer_t next_buffer(uint32_t s, const B* dummy = 0);

    template <class B, int N>
    void split_buffers(pn_raw_buffer_t (&buffers)[N]) {
      uint32_t buffsize  = (size-brk)/N;
      uint32_t remainder = (size-brk)%N;
      for (int i = 0; i<N; ++i) {
        buffers[i] = next_buffer<B>(i==0 ? buffsize+remainder : buffsize);
      }
    }
  };

  template <>
  pn_raw_buffer_t BufferAllocator::next_buffer(uint32_t s, const ReadBuffer*) {
    pn_raw_buffer_t b = {};
    b.bytes = next(s);
    if (b.bytes) {b.capacity = s; b.size = 0;}
    return b;
  }

  template <>
  pn_raw_buffer_t BufferAllocator::next_buffer(uint32_t s, const WriteBuffer*) {
    pn_raw_buffer_t b = {};
    b.bytes = next(s);
    if (b.bytes) {b.capacity = s; b.size = s;}
    return b;
  }
}

char message[] =
"Jabberwocky\n"
"By Lewis Carroll\n"
"\n"
"'Twas brillig, and the slithy toves\n"
"Did gyre and gimble in the wabe:\n"
"All mimsy were the borogroves,\n"
"And the mome raths outgrabe.\n"
"\n"
"Beware the Jabberwock, my son!\n"
"The jaws that bite, the claws that catch!\n"
"Beware the Jubjub bird, and shun\n"
"The frumious Bandersnatch!\n"
"\n"
"He took his vorpal sword in hand;\n"
"Long time the manxome foe he sought-\n"
"So rested he by the Tumtum tree\n"
"And stood awhile in thought.\n"
"\n"
"And, as in uffish thought he stood,\n"
"The Jabberwock with eyes of flame,\n"
"Came whiffling through the tulgey wood,\n"
"And burbled as it came!\n"
"\n"
"One, two! One, two! And through and through,\n"
"The vorpal blade went snicker-snack!\n"
"He left it dead, and with its head\n"
"He went galumphing back.\n"
"\n"
"\"And hast thou slain the JabberWock?\n"
"Come to my arms, my beamish boy!\n"
"O frabjous day! Callooh! Callay!\"\n"
"He chortled in his joy.\n"
"\n"
"'Twas brillig, and the slithy toves\n"
"Did gyre and gimble in the wabe:\n"
"All mimsy were the borogroves,\n"
"And the mome raths outgrabe.\n"
;

TEST_CASE("raw connection") {
  auto_free<pn_raw_connection_t, pn_raw_connection_free> p(pn_raw_connection());

  REQUIRE(p);
  REQUIRE(pni_raw_validate(p));
  REQUIRE(!pn_raw_connection_is_read_closed(p));
  REQUIRE(!pn_raw_connection_is_write_closed(p));

  int rbuff_count = pn_raw_connection_read_buffers_capacity(p);
  REQUIRE(rbuff_count>0);
  int wbuff_count = pn_raw_connection_write_buffers_capacity(p);
  REQUIRE(wbuff_count>0);

  BufferAllocator rb(rbuffer_memory, sizeof(rbuffer_memory));
  BufferAllocator wb(message, sizeof(message));

  rb.split_buffers<ReadBuffer>(rbuffs);
  wb.split_buffers<WriteBuffer>(wbuffs);

  int rtaken = pn_raw_connection_give_read_buffers(p, rbuffs, RBUFFCOUNT);
  REQUIRE(pni_raw_validate(p));
  REQUIRE(rtaken==rbuff_count);

  SECTION("Write multiple per event loop") {
    int wtaken = 0;
    for (size_t i = 0; i < WBUFFCOUNT; ++i) {
      int taken = pn_raw_connection_write_buffers(p, &wbuffs[i], 1);
      if (taken==0) break;
      REQUIRE(pni_raw_validate(p));
      REQUIRE(taken==1);
      wtaken += taken;
    }

    REQUIRE(pn_raw_connection_read_buffers_capacity(p) == 0);
    REQUIRE(pn_raw_connection_write_buffers_capacity(p) == 0);

    std::vector<pn_raw_buffer_t> read(rtaken);
    std::vector<pn_raw_buffer_t> written(wtaken);

    SECTION("Simple tests using a looped back socketpair") {
      int fds[2];
      REQUIRE(makepair(fds) == 0);
      pni_raw_connected(p);

      // First event is always connected
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CONNECTED);
      // Mo need buffers event as we already gave buffers
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);

      SECTION("Write then read") {
        pni_raw_write(p, fds[0], snd, set_write_error);
        REQUIRE(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        REQUIRE(pn_raw_connection_write_buffers_capacity(p) == 0);
        int wgiven = pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(wgiven==wtaken);

        // Write more
        for (size_t i = wtaken; i < WBUFFCOUNT; ++i) {
          int taken = pn_raw_connection_write_buffers(p, &wbuffs[i], 1);
          if (taken==0) break;
          REQUIRE(pni_raw_validate(p));
          REQUIRE(taken==1);
          wtaken += taken;
        }

        pni_raw_write(p, fds[0], snd, set_write_error);
        REQUIRE(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
        wgiven += pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        REQUIRE(wgiven==wtaken);

        // At this point we've written every buffer
        REQUIRE(pn_raw_connection_write_buffers_capacity(p) == wbuff_count);

        pni_raw_read(p, fds[1], rcv, set_read_error);
        REQUIRE(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        REQUIRE(pn_raw_connection_read_buffers_capacity(p) == 0);
        int rgiven = pn_raw_connection_take_read_buffers(p, &read[0], read.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(rgiven > 0);

        REQUIRE(pn_raw_connection_read_buffers_capacity(p) == rgiven);

        // At this point we should have read everything - make sure it matches
        char* start = message;
        for (int i = 0; i < rgiven; ++i) {
          REQUIRE(read[i].size > 0);
          REQUIRE(std::string(read[i].bytes, read[i].size) == std::string(start, read[i].size));
          start += read[i].size;
        }
        REQUIRE(start-message == sizeof(message));
      }

    }
  }

  SECTION("Write once per event loop") {
    int wtaken = pn_raw_connection_write_buffers(p, wbuffs, WBUFFCOUNT);
    REQUIRE(pni_raw_validate(p));
    REQUIRE(wtaken==wbuff_count);

    REQUIRE(pn_raw_connection_read_buffers_capacity(p) == 0);
    REQUIRE(pn_raw_connection_write_buffers_capacity(p) == 0);

    std::vector<pn_raw_buffer_t> read(rtaken);
    std::vector<pn_raw_buffer_t> written(wtaken);

    SECTION("Check no change in buffer use without read/write") {

      int rgiven = pn_raw_connection_take_read_buffers(p, &read[0], rtaken);
      REQUIRE(pni_raw_validate(p));
      REQUIRE(rgiven==0);
      int wgiven = pn_raw_connection_take_written_buffers(p, &written[0], wtaken);
      REQUIRE(pni_raw_validate(p));
      REQUIRE(wgiven==0);
    }

    SECTION("Simple tests using a looped back socketpair") {
      int fds[2];
      REQUIRE(makepair(fds) == 0);
      pni_raw_connected(p);

      // First event is always connected
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CONNECTED);
      // Mo need buffers event as we already gave buffers
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);

      SECTION("Ensure nothing is read if nothing is written") {
        pni_raw_read(p, fds[1], rcv, set_read_error);
        REQUIRE(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_raw_connection_read_buffers_capacity(p) == 0);
        REQUIRE(pn_raw_connection_take_read_buffers(p, &read[0], read.size()) == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pni_raw_event_next(p) == NULL);

        snd_stop(fds[0]);
        pni_raw_read(p, fds[1], rcv, set_read_error);
        REQUIRE(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_raw_connection_is_read_closed(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CLOSED_READ);
        rcv_stop(fds[1]);
        pni_raw_write(p, fds[0], snd, set_write_error);
        REQUIRE(write_err == EPIPE);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_raw_connection_is_write_closed(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CLOSED_WRITE);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_DISCONNECTED);
      }

      SECTION("Read/Write interleaved") {
        pni_raw_write(p, fds[0], snd, set_write_error);
        REQUIRE(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_raw_connection_write_buffers_capacity(p) == 0);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        int wgiven = pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        REQUIRE(wgiven==wtaken);
        REQUIRE(pn_raw_connection_write_buffers_capacity(p) == wbuff_count);

        pni_raw_read(p, fds[1], rcv, set_read_error);
        REQUIRE(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_raw_connection_read_buffers_capacity(p) == 0);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        int rgiven = pn_raw_connection_take_read_buffers(p, &read[0], read.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(rgiven > 0);
        REQUIRE(pn_raw_connection_read_buffers_capacity(p) == rgiven);

        // Write more
        wtaken += pn_raw_connection_write_buffers(p, &wbuffs[wtaken], WBUFFCOUNT-wtaken);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(wtaken==WBUFFCOUNT);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);

        pni_raw_write(p, fds[0], snd, set_write_error);
        REQUIRE(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        wgiven += pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
        REQUIRE(wgiven==wtaken);

        // At this point we've written every buffer

        pni_raw_read(p, fds[1], rcv, set_read_error);
        REQUIRE(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        int rgiven_before = rgiven;
        rgiven += pn_raw_connection_take_read_buffers(p, &read[rgiven], read.size()-rgiven);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(rgiven > rgiven_before);

        REQUIRE(pn_raw_connection_read_buffers_capacity(p) == rgiven);
        REQUIRE(pn_raw_connection_write_buffers_capacity(p) == wbuff_count);

        // At this point we should have read everything - make sure it matches
        char* start = message;
        for (int i = 0; i < rgiven; ++i) {
          REQUIRE(read[i].size > 0);
          REQUIRE(std::string(read[i].bytes, read[i].size) == std::string(start, read[i].size));
          start += read[i].size;
        }
        REQUIRE(start-message == sizeof(message));
      }

      SECTION("Write then read") {
        pni_raw_write(p, fds[0], snd, set_write_error);
        REQUIRE(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        REQUIRE(pn_raw_connection_write_buffers_capacity(p) == 0);
        int wgiven = pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(wgiven==wtaken);

        // Write more
        wtaken += pn_raw_connection_write_buffers(p, &wbuffs[wtaken], WBUFFCOUNT-wtaken);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(wtaken==WBUFFCOUNT);

        pni_raw_write(p, fds[0], snd, set_write_error);
        REQUIRE(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
        wgiven += pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        REQUIRE(wgiven==wtaken);

        // At this point we've written every buffer
        REQUIRE(pn_raw_connection_write_buffers_capacity(p) == wbuff_count);

        pni_raw_read(p, fds[1], rcv, set_read_error);
        REQUIRE(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        REQUIRE(pn_raw_connection_read_buffers_capacity(p) == 0);
        int rgiven = pn_raw_connection_take_read_buffers(p, &read[0], read.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(rgiven > 0);

        REQUIRE(pn_raw_connection_read_buffers_capacity(p) == rgiven);

        // At this point we should have read everything - make sure it matches
        char* start = message;
        for (int i = 0; i < rgiven; ++i) {
          REQUIRE(read[i].size > 0);
          REQUIRE(std::string(read[i].bytes, read[i].size) == std::string(start, read[i].size));
          start += read[i].size;
        }
        REQUIRE(start-message == sizeof(message));
      }

      SECTION("Write, close, then read") {
        pni_raw_write(p, fds[0], snd, set_write_error);
        REQUIRE(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        REQUIRE(pn_raw_connection_write_buffers_capacity(p) == 0);
        int wgiven = pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(wgiven==wtaken);

        // Write more
        wtaken += pn_raw_connection_write_buffers(p, &wbuffs[wtaken], WBUFFCOUNT-wtaken);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(wtaken==WBUFFCOUNT);

        pni_raw_write(p, fds[0], snd, set_write_error);
        REQUIRE(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
        wgiven += pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        REQUIRE(wgiven==wtaken);

        // At this point we've written every buffer
        REQUIRE(pn_raw_connection_write_buffers_capacity(p) == wbuff_count);

        snd_stop(fds[0]);
        pni_raw_read(p, fds[1], rcv, set_read_error);
        REQUIRE(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CLOSED_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        REQUIRE(pn_raw_connection_read_buffers_capacity(p) == 0);
        int rgiven = pn_raw_connection_take_read_buffers(p, &read[0], read.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(rgiven > 0);

        REQUIRE(pn_raw_connection_read_buffers_capacity(p) == rgiven);
        REQUIRE(read[rgiven-1].size == 0);

        // At this point we should have read everything - make sure it matches
        char* start = message;
        for (int i = 0; i < rgiven-1; ++i) {
          REQUIRE(read[i].size > 0);
          REQUIRE(std::string(read[i].bytes, read[i].size) == std::string(start, read[i].size));
          start += read[i].size;
        }
        REQUIRE(start-message == sizeof(message));
      }

      freepair(fds);
    }
  }
}
