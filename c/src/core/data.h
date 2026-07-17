#ifndef _PROTON_DATA_H
#define _PROTON_DATA_H 1

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

#include <proton/codec.h>
#include "buffer.h"
#include "decoder.h"
#include "encoder.h"

typedef uint16_t pni_nid_t;
#define PNI_NID_MAX ((pni_nid_t)-1)
#define PNI_INTERN_MINSIZE 64

/*
 * Value payload for a pni_node_t.
 *
 * BINARY/STRING/SYMBOL/DECIMAL128/UUID nodes store their data in the intern
 * buffer (data->buf); as_bytes.offset and as_bytes.size locate the bytes.
 * DECIMAL128 and UUID always have as_bytes.size == 16.
 *
 * PN_ARRAY nodes store the element type in array_type.
 *
 * All other types (NULL, LIST, MAP, DESCRIBED) carry no payload; only the
 * type tag on pni_node_t is meaningful.
 */
typedef union {
  bool            as_bool;
  uint8_t         as_ubyte;
  int8_t          as_byte;
  uint16_t        as_ushort;
  int16_t         as_short;
  uint32_t        as_uint;
  int32_t         as_int;
  uint32_t        as_char;        /* pn_char_t is typedef'd uint32_t */
  uint64_t        as_ulong;
  int64_t         as_long;
  int64_t         as_timestamp;   /* pn_timestamp_t is typedef'd int64_t */
  float           as_float;
  double          as_double;
  uint32_t        as_decimal32;
  uint64_t        as_decimal64;
  struct {
    uint32_t      offset;         /* byte offset into data->buf */
    uint32_t      size;           /* byte count (always 16 for decimal128/uuid) */
  }               as_bytes;
  pn_type_t       array_type;     /* PN_ARRAY element type */
} pni_node_payload_t;

/*
 * Layout (64-bit): 32 bytes, no padding.
 *
 *  offset  0  type        (4)  value type tag
 *  offset  4  described   (1)  PN_ARRAY: has descriptor child
 *  offset  5  small       (1)  encoder scratch
 *  offset  6  next        (2)  fills alignment gap before u
 *  offset  8  u           (8)  value payload (8-byte aligned)
 *  offset 16  start       (8)  encoder scratch (8-byte aligned)
 *  offset 24  prev        (2)
 *  offset 26  down        (2)
 *  offset 28  parent      (2)
 *  offset 30  children    (2)
 */
typedef struct {
  pn_type_t           type;
  bool                described;
  bool                small;
  pni_nid_t           next;
  pni_node_payload_t  u;
  size_t              start;
  pni_nid_t           prev;
  pni_nid_t           down;
  pni_nid_t           parent;
  pni_nid_t           children;
} pni_node_t;

struct pn_data_t {
  pni_node_t *nodes;
  pn_buffer_t *buf;
  pn_error_t *error;
  size_t max_buf_size; /* intern buffer limit during decode; 0 = unlimited */
  pni_nid_t max_nid;   /* node count limit during decode; 0 = unlimited */
  pni_nid_t capacity;
  pni_nid_t size;
  pni_nid_t parent;
  pni_nid_t current;
  pni_nid_t base_parent;
  pni_nid_t base_current;
};

/* Node-count decode limits passed to pni_switch_to_data().
 *
 * The intern buffer limit is always bytes->size (handled inside
 * pni_switch_to_data itself): interned strings must originate from the raw
 * bytes being decoded, so 1:1 is a tight bound that prevents amplification
 * without imposing an artificial ceiling on any field.
 *
 * The node count needs a separate constant because 0-width elements (e.g.
 * PNE_NULL in an array) consume a node but no bytes, so bytes->size does not
 * bound node count.
 *
 * PNI_DATA_DEFAULT_MAX_NODES (1024): performative fields — connection/terminus/
 * link properties and capabilities, condition info, disposition data, message
 * annotations and application-properties. These carry structured protocol
 * metadata; values exceeding this limit indicate malformed or malicious input.
 *
 * PNI_DATA_BODY_MAX_NODES (0 = unlimited): message body — application data
 * whose node count is bounded only by the uint16 hard ceiling of PNI_NID_MAX.
 * The body bytes are already bounded by the transport's
 * max_buffered_delivery_bytes limit. */
#define PNI_DATA_DEFAULT_MAX_NODES 1024
#define PNI_DATA_BODY_MAX_NODES    0

static inline pni_node_t * pn_data_node(pn_data_t *data, pni_nid_t nd)
{
  return nd ? (data->nodes + nd - 1) : NULL;
}

int pni_data_traverse(pn_data_t *data,
                      int (*enter)(void *ctx, pn_data_t *data, pni_node_t *node),
                      int (*exit)(void *ctx, pn_data_t *data, pni_node_t *node),
                      void *ctx);

struct pn_fixed_string_t;
void pni_inspect_atom(pn_atom_t *atom, struct pn_fixed_string_t *str);

#endif /* data.h */
