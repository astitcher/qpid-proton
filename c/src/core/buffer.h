#ifndef PROTON_BUFFER_H
#define PROTON_BUFFER_H 1

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

#include <proton/import_export.h>
#include <proton/types.h>

#include "core/object_private.h"

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pn_buffer_t pn_buffer_t;

pn_buffer_t *pn_buffer(size_t capacity);
void pn_buffer_free(pn_buffer_t *buf);
size_t pn_buffer_size(pn_buffer_t *buf);
size_t pn_buffer_capacity(pn_buffer_t *buf);
size_t pn_buffer_available(pn_buffer_t *buf);
int pn_buffer_ensure(pn_buffer_t *buf, size_t size);
int pn_buffer_append(pn_buffer_t *buf, const char *bytes, size_t size);
size_t pn_buffer_get(pn_buffer_t *buf, size_t offset, size_t size, char *dst);
int pn_buffer_trim(pn_buffer_t *buf, size_t left, size_t right);
void pn_buffer_clear(pn_buffer_t *buf);
pn_bytes_t pn_buffer_bytes(pn_buffer_t *buf);
pn_rwbytes_t pn_buffer_memory(pn_buffer_t *buf);

typedef struct pn_buffer_list_t pn_buffer_list_t;


typedef struct pn_buffer_list_entry_t {
    uint8_t *buffer;
    uint32_t size;
    uint32_t offset;
} pn_buffer_list_entry_t;

typedef void pn_buffer_list_entry_free_cb(uintptr_t, pn_buffer_list_entry_t*);

// Up to 15 direct buffer entries held here, if there are more than 15 then indirect points
// to another buffer list array which is resized if needed to keep all the buffers.
// The buffer list is maintained as a circular queue of buffers so:
// if entry_head == entry_tail the list is empty
// if entry_head+1 % entry_count == entry_tail the list is full
//       entry_head+1 == entry_count    where entry_tail == 0
//    or entry_head+1 == entry_tail     where entry_head < entry_tail
// if entry_head < entry_tail then we've wrapped around
//
struct pn_buffer_list_t {
  uintptr_t free_context;
  pn_buffer_list_entry_free_cb *free_cb;
  pn_buffer_list_entry_t *indirect;
  uint16_t entry_count; // Total count of entries
  uint16_t entry_head;  // Buffer appends go here
  uint16_t entry_tail;  // Buffer gets come from here
  // 2 bytes padding here
  pn_buffer_list_entry_t direct[15];
};

static inline pn_buffer_list_entry_t *pn_buffer_list_get_entry(pn_buffer_list_t *blist, uint16_t index) {
  // If we don't have enough buffers return NULL
  if (blist->entry_count<index) return NULL;

  // If we have no indirect block or the index is in the direct block
  if (blist->entry_count<=15 || index < 15) return &blist->direct[index];

  // else go through the indirect block
  uint16_t indirect_index = index-15;
  pn_buffer_list_entry_t *indirect_base = blist->indirect;
  return &indirect_base[indirect_index];
}

static inline bool pn_buffer_list_full(pn_buffer_list_t *blist)
{
  return blist->entry_tail == 0
    ? blist->entry_head+1 == blist->entry_count
    : blist->entry_head+1 == blist->entry_tail;
}

static inline bool pn_buffer_list_empty(pn_buffer_list_t *blist)
{
  return blist->entry_head == blist->entry_tail;
}

// TODO: Do it a different way that works with C++
#ifndef __cplusplus
static inline pn_buffer_list_entry_t pn_buffer_list_entry(const uint8_t *buffer, uint32_t size, uint32_t offset)
{
    return (pn_buffer_list_entry_t){.buffer=(uint8_t*)buffer, .size=size, .offset=offset};
}

static inline pn_buffer_list_entry_t pn_buffer_list_entry_from_bytes(pn_bytes_t bytes)
{
  return (pn_buffer_list_entry_t){.buffer=(uint8_t*)bytes.start, .size=bytes.size, .offset=0};
}

static const pn_buffer_list_entry_t pn_buffer_list_entry_null = {.buffer=NULL, .size=0, .offset=0};

static inline pn_bytes_t pn_bytes_from_buffer_list_entry(pn_buffer_list_entry_t *entry)
{
  return entry ? (pn_bytes_t){.size=entry->size, .start=(char*)entry->buffer+entry->offset} : (pn_bytes_t){0, NULL};
}
#endif

static inline void pn_buffer_list_entry_free(pn_buffer_list_entry_t *entry)
{
  free((void*)entry->buffer);
}

static inline void pn_buffer_list_advance_head(pn_buffer_list_t *blist)
{
  if (blist->entry_head+1==blist->entry_count) {
    blist->entry_head = 0;
  } else {
    ++blist->entry_head;
  }
}

static inline void pn_buffer_list_advance_tail(pn_buffer_list_t *blist)
{
  // This first case is to avoid unnecessary tail copying on list resize
  if (blist->entry_tail+1==blist->entry_head) {
    blist->entry_tail = 0;
    blist->entry_head = 0;
  } else if (blist->entry_tail+1==blist->entry_count) {
    blist->entry_tail = 0;
  } else {
    ++blist->entry_tail;
  }
}

void pn_buffer_list_init(pn_buffer_list_t *blist, pn_buffer_list_entry_free_cb *free_cb, uintptr_t context);
void pn_buffer_list_clear(pn_buffer_list_t *blist);
uint32_t pn_buffer_list_byte_total(pn_buffer_list_t *blist);
bool pn_buffer_list_has_data(pn_buffer_list_t *blist);
void pn_buffer_list_append_head(pn_buffer_list_t *blist, pn_buffer_list_entry_t entry);
pn_buffer_list_entry_t *pn_buffer_list_tail(pn_buffer_list_t *blist);
size_t pn_buffer_list_output(pn_buffer_list_t *blist, char *bytes, size_t n);
size_t pn_buffer_list_transfer(pn_buffer_list_t *input, pn_buffer_list_t *output);


#ifdef __cplusplus
}
#endif

#endif /* buffer.h */
