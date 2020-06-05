#ifndef PROACTOR_RAW_CONNECTION_INTERNAL_H
#define PROACTOR_RAW_CONNECTION_INTERNAL_H
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

enum {
  read_buffer_count = 16,
  write_buffer_count = 16
};

typedef enum {
  buff_rempty    = 0,
  buff_unread    = 1,
  buff_read      = 2,
  buff_wempty    = 4,
  buff_unwritten = 5,
  buff_written   = 6
} buff_type;

typedef uint16_t buff_ptr; // This is always the index+1 so that 0 can be special

typedef struct pbuffer_t {
  uintptr_t context;
  char *bytes;
  uint32_t capacity;
  uint32_t size;
  uint32_t offset;
  buff_ptr next;
  uint8_t type; // For debugging
} pbuffer_t;

struct pn_raw_connection_t {
  pbuffer_t rbuffers[read_buffer_count];
  pbuffer_t wbuffers[write_buffer_count];
  pn_condition_t *condition;
  pn_collector_t *collector;
  pn_record_t *attachments;
  uint16_t rbuffer_count;
  uint16_t wbuffer_count;

  buff_ptr rbuffer_first_empty;
  buff_ptr rbuffer_first_unused;
  buff_ptr rbuffer_last_unused;
  buff_ptr rbuffer_first_read;
  buff_ptr rbuffer_last_read;

  buff_ptr wbuffer_first_empty;
  buff_ptr wbuffer_first_towrite;
  buff_ptr wbuffer_last_towrite;
  buff_ptr wbuffer_first_written;
  buff_ptr wbuffer_last_written;
  bool rneedbufferevent;
  bool wneedbufferevent;
  bool rpending;
  bool wpending;
  bool rclosed;
  bool wclosed;
  bool rclosedpending;
  bool wclosedpending;
  bool disconnectpending;
  bool freed;
  bool freeable;
};

#endif // PROACTOR_RAW_CONNECTION_INTERNAL_H
