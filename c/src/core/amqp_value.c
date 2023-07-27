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

#include "amqp_value_private.h"
#include "consumers.h"
#include "emitters.h"
#include "encodings.h"
#include "value_dump.h"

#include <proton/annotations.h>
#include <proton/amqp_value.h>

#include <stdlib.h>

// Every type that can represent an amqp protocol type
// that can be treated as a pn_amqp_value_t must have the pn_type_t
// member as the first member
PN_PUSH_WARNING
PN_MSVC_DISABLE_WARNING(4200)
struct pn_amqp_compound_t {
  pn_amqp_compund_type_t compound_type;
  uint32_t raw_size;
  uint32_t items_size;
  uint32_t items_count;
  uint8_t  raw[];
};
PN_POP_WARNING

size_t pni_string_encoded_size(pn_bytes_t s) {
  size_t len = s.size;
  // typecode + len (either 1 or 4 bytes) + string length
  return len + 1 + (len < 256 ? 1 : 4);
}

size_t pn_amqp_value_encoded_size(pn_amqp_value_t v) {
  switch (v.value_type) {
    case PN_NULL:
    case PN_BOOL:
      return 1;
    case PN_UBYTE:
      return 2;
    case PN_USHORT:
      return 3;
    case PN_UINT:
      if (v.u.as_uint == 0) {
        return 1;
      } else if (v.u.as_uint < 256) {
        return 2;
      } else {
        return 5;
      }
    case PN_ULONG:
      if (v.u.as_ulong == 0) {
        return 1;
      } else if (v.u.as_ulong < 256) {
        return 2;
      } else {
        return 9;
      }
    case PN_BYTE:
      return 2;
    case PN_SHORT:
      return 3;
    case PN_INT:
      if (v.u.as_int > -128 && v.u.as_int < 128) {
        return 2;
      } else {
        return 5;
      }
    case PN_LONG:
      if (v.u.as_long > -128 && v.u.as_long < 128) {
        return 2;
      } else {
        return 9;
      }
    case PN_CHAR:
    case PN_DECIMAL32:
    case PN_FLOAT:
      return 5;
    case PN_DECIMAL64:
    case PN_DOUBLE:
    case PN_TIMESTAMP:
      return 9;
    case PN_DECIMAL128:
    case PN_UUID:
      return 17;
    case PN_BINARY:
    case PN_STRING:
    case PN_SYMBOL: {
      size_t len = v.u.as_bytes.size;
      return len + 1 + (len < 256 ? 1 : 4);
    }
    case PN_DESCRIBED:
    case PN_ARRAY:
    case PN_LIST:
    case PN_MAP: {
      return v.u.as_bytes.size;
    }
    case PN_INVALID:
    default:
      return 0;
  }
}

static void emit_amqp_value(pni_emitter_t *emitter, pni_compound_context *compound, pn_amqp_value_t *v) {
  switch ((int)v->value_type) {
    case PN_DESCRIBED:
    case PN_ARRAY:
    case PN_LIST:
    case PN_MAP:
    case PN_COMPOUND_MAP:
    case PN_COMPOUND_LIST:
    case PN_COMPOUND_ARRAY:
      emit_raw(emitter, compound, pn_amqp_value_bytes(v));
      return;
    default:
      emit_atom(emitter, compound, (pn_atom_t*) v);
  }

}

static inline pn_type_t pni_amqp_typecode_to_type(uint8_t typecode)
{
  switch (typecode) {
    case PNE_DESCRIPTOR:
      return PN_DESCRIBED;
    case PNE_MAP8:
    case PNE_MAP32:
      return PN_MAP;
    case PNE_LIST8:
    case PNE_LIST32:
      return PN_LIST;
    case PNE_ARRAY8:
    case PNE_ARRAY32:
      return PN_ARRAY;
    default:
      return PN_INVALID;
  }
}

static bool consume_amqp_value(pni_consumer_t* consumer, pn_amqp_value_t *value) {
  pni_consumer_t c = *consumer;
  uint8_t type;
  if (pni_consumer_readf8(&c, &type)) {
    switch (type) {
      case PNE_DESCRIPTOR:
      case PNE_ARRAY8:
      case PNE_ARRAY32:
      case PNE_LIST0:
      case PNE_LIST8:
      case PNE_LIST32:
      case PNE_MAP8:
      case PNE_MAP32: {
        if (consume_raw(consumer, &value->u.as_bytes)) {
          value->value_type = pni_amqp_typecode_to_type(type);
          return true;
        }
        return false;
      }
      default:
        return consume_atom(consumer, (pn_atom_t *)value);
    }
  }
  *consumer = c;
  return false;
}

static inline bool pni_small_encoding(uint8_t typecode) {
  return typecode==PNE_MAP8 || typecode==PNE_LIST8 || typecode==PNE_ARRAY8;
}

static inline pn_amqp_compund_type_t pni_amqp_compound_typecode_to_type(uint8_t typecode)
{
  switch (typecode) {
    case PNE_MAP8:
    case PNE_MAP32:
      return PN_COMPOUND_MAP;
    case PNE_LIST8:
    case PNE_LIST32:
      return PN_COMPOUND_LIST;
    case PNE_ARRAY8:
    case PNE_ARRAY32:
      return PN_COMPOUND_ARRAY;
    default:
      return (pn_amqp_compund_type_t) PN_INVALID;
  }
}

static inline uint8_t pni_amqp_compound_type_to_typecode(pn_amqp_compund_type_t type, bool small_encoding) {
    switch(type) {
      case PN_COMPOUND_MAP:
        return small_encoding ? PNE_MAP8 : PNE_MAP32;
      case PN_COMPOUND_LIST:
        return small_encoding ? PNE_LIST8 : PNE_LIST32;
      case PN_COMPOUND_ARRAY:
        return small_encoding ? PNE_ARRAY8 : PNE_ARRAY32;
      default:
        return PN_INVALID;
    }
}

static inline pn_amqp_compound_t *pni_amqp_compound_new(size_t raw_size) {
  return malloc(offsetof(pn_amqp_compound_t, raw) + raw_size);
}

pn_bytes_t pni_amqp_compound_bytes(pn_amqp_compound_t *compound) {
  if (!compound) return (pn_bytes_t){0, NULL};
  bool small_encoding = compound->items_size<255 && compound->items_count<256;
  uint8_t offset = small_encoding ? 6 : 0;
  return (pn_bytes_t){.size=compound->items_size+9-offset, .start=(const char*)&compound->raw[offset]};
}

pn_bytes_t pn_amqp_value_bytes(pn_amqp_value_t *value) {
  switch ((int)value->value_type) {
    case PN_DESCRIBED:
    case PN_MAP:
    case PN_LIST:
    case PN_ARRAY:
      return value->u.as_bytes;
    case PN_COMPOUND_MAP:
    case PN_COMPOUND_LIST:
    case PN_COMPOUND_ARRAY:
      return pni_amqp_compound_bytes((pn_amqp_compound_t*)value);
    default:
      return (pn_bytes_t){.size=0, .start=NULL};
  }
}

char *pn_amqp_value_tostring(pn_amqp_value_t *value) {
  char buf[1024];
  size_t size = pn_value_dump(pn_amqp_value_bytes(value), buf, sizeof(buf));
  char *r = malloc(size+1);
  strncpy(r, buf, size+1);
  return r;
}

void pn_amqp_value_free(pn_amqp_value_t *value) {
  free(value);
}

pn_amqp_compound_t *pn_amqp_compound_make(pn_bytes_t raw) {
  uint32_t raw_size = raw.size;
  if (raw_size==0) return NULL;

  uint32_t offset;
  uint32_t items_size;
  uint32_t items_count;
  pni_consumer_t consumer = make_consumer_from_bytes(raw);
  uint8_t typecode;
  // This must succeed as we have at least 1 byte to read
  pni_consumer_readf8(&consumer, &typecode);

  if (pni_small_encoding(typecode)) {
    // Leave space for large encoding in case we expand later
    offset = 6;
    raw_size += offset;
    uint8_t size;
    uint8_t count;
    if (
      !pni_consumer_readf8(&consumer, &size) ||
      !pni_consumer_readf8(&consumer, &count)
    ) return NULL;

    items_size = size-1;
    items_count = count;
  } else {
    offset = 0;
    if (
      !pni_consumer_readf32(&consumer, &items_size) ||
      !pni_consumer_readf32(&consumer, &items_count)
    ) return NULL;

    items_size -= 4;
  }
  pn_amqp_compound_t *c = pni_amqp_compound_new(raw_size);
  c->raw_size = raw_size;
  c->items_size = items_size;
  c->items_count = items_count;
  c->compound_type = pni_amqp_compound_typecode_to_type(typecode);
  memcpy(&c->raw[offset], &raw.start[0], raw_size-offset);
  return c;
}

struct size_count {uint32_t size; uint32_t count;};
typedef struct size_count (*items_vsize)(va_list);
typedef void (*items_vemit)(pni_emitter_t*, va_list);
static inline pn_amqp_compound_t *pn_amqp_compound_build(pn_amqp_compound_t *compound, pn_amqp_compund_type_t type, items_vsize vsize, items_vemit vemit, va_list items) {
  // Figure out extra size of new items
  va_list items2;
  va_copy(items2, items);

  struct size_count r = vsize(items);
  size_t  size = r.size;
  uint32_t count = r.count;


  pn_amqp_compound_t *c = (pn_amqp_compound_t*)compound;
  uint32_t old_items_size  = c ? c->items_size : 0;
  uint32_t new_items_size  = old_items_size + size;

  if (!c) {
    c = pni_amqp_compound_new(new_items_size+9);
    c->raw_size = new_items_size+9;
    c->items_count = 0;
    c->compound_type = type;
    // Now we can resize the raw space (if necessary)
  } else if (c->raw_size < new_items_size) {
    c = realloc(compound, offsetof(pn_amqp_compound_t, raw) + new_items_size + 9);
    // Realloc failed
    if (!c) {
        free(compound);
        return c;
    }
    c->raw_size = new_items_size + 9;
  }
  c->items_size  = new_items_size;
  c->items_count += count;

  // Write header
  bool small_encoding = c->items_size<255 && c->items_count<256;
  if (small_encoding) {
    pn_rwbytes_t bytes = {.size=3, .start=(char*)&c->raw[6]};
    pni_emitter_t emitter = make_emitter_from_bytes(bytes);
    pni_emitter_writef8(&emitter, pni_amqp_compound_type_to_typecode(c->compound_type, small_encoding));
    // Have to include count in size here
    pni_emitter_writef8(&emitter, c->items_size+1);
    pni_emitter_writef8(&emitter, c->items_count);
  } else {
    pn_rwbytes_t bytes = {.size=9, .start=(char*)&c->raw[0]};
    pni_emitter_t emitter = make_emitter_from_bytes(bytes);
    pni_emitter_writef8(&emitter, pni_amqp_compound_type_to_typecode(c->compound_type, small_encoding));
    // Have to include count in size here
    pni_emitter_writef32(&emitter, c->items_size+4);
    pni_emitter_writef32(&emitter, c->items_count);
  }

  // Write new items
  pn_rwbytes_t bytes = {.size=size, .start=(char*)&c->raw[old_items_size+9]};
  pni_emitter_t emitter = make_emitter_from_bytes(bytes);
  vemit(&emitter, items2);
  va_end(items2);
  return c;
}

pn_bytes_t pn_amqp_map_bytes(pn_amqp_map_t *map) {
  return pni_amqp_compound_bytes((pn_amqp_compound_t*)map);
}

pn_amqp_iterator_t pn_amqp_map_iterator(pn_amqp_map_t *map) {
  pn_amqp_compound_t *c = (pn_amqp_compound_t*)map;
  if (!c)
    return (pn_amqp_iterator_t){.start=NULL, .size=0, .position=0};
  return (pn_amqp_iterator_t){.start=&c->raw[9], .size=c->items_size, .position=0};
}

void pn_amqp_map_free(pn_amqp_map_t *map) {
  free(map);
}

pn_bytes_t pn_amqp_list_bytes(pn_amqp_list_t *list) {
  return pni_amqp_compound_bytes((pn_amqp_compound_t*)list);
}

pn_amqp_iterator_t pn_amqp_list_iterator(pn_amqp_list_t *list) {
  pn_amqp_compound_t *c = (pn_amqp_compound_t*)list;
  if (!c)
    return (pn_amqp_iterator_t){.start=NULL, .size=0, .position=0};
  return (pn_amqp_iterator_t){.start=&c->raw[9], .size=c->items_size, .position=0};
}

void pn_amqp_list_free(pn_amqp_list_t *list) {
  free(list);
}

pn_bytes_t pn_amqp_array_bytes(pn_amqp_array_t *array) {
  return pni_amqp_compound_bytes((pn_amqp_compound_t*)array);
}

pn_amqp_iterator_t pn_amqp_array_iterator(pn_amqp_array_t *array) {
  pn_amqp_compound_t *c = (pn_amqp_compound_t*)array;
  if (!c)
    return (pn_amqp_iterator_t){.start=NULL, .size=0, .position=0};

  // Skip element typecode
  uint8_t element_type = c->raw[9];
  // We won't handle described types - so set position at end of iterator
  if (element_type==PNE_DESCRIPTOR)
    return (pn_amqp_iterator_t){.start=&c->raw[9], .size=c->items_size, .position=c->items_size};
  return (pn_amqp_iterator_t){.start=&c->raw[9], .size=c->items_size, .position=1};
}

uint32_t pn_amqp_array_count(pn_amqp_array_t *array) {
  pn_amqp_compound_t *c = (pn_amqp_compound_t*)array;
  return c->items_count;
}

void pn_amqp_array_free(pn_amqp_array_t *array) {
  free(array);
}

struct size_count pni_property_items_vsize(va_list items) {
  uint32_t size = 0;
  uint32_t count = 0;

  pn_bytes_t key = va_arg(items, pn_bytes_t);
  while (key.size!=0) {
    pn_amqp_value_t val = va_arg(items, pn_amqp_value_t);

    size += pni_string_encoded_size(key);
    size += pn_amqp_value_encoded_size(val);
    count += 2;

    key = va_arg(items, pn_bytes_t);
  }
  return (struct size_count){size, count};
}

void pni_property_items_vemit(pni_emitter_t *emitter, va_list items) {
  pni_compound_context compound = make_compound();
  pn_bytes_t key = va_arg(items, pn_bytes_t);
  while (key.size!=0) {
    pn_atom_t val = va_arg(items, pn_atom_t);
    emit_string_bytes(emitter, &compound, key);
    emit_atom(emitter, &compound, &val);
    key = va_arg(items, pn_bytes_t);
  }
}

pn_amqp_map_t *pn_message_properties_build(pn_amqp_map_t *properties, ...) {
  va_list items;
  va_start(items, properties);
  pn_amqp_compound_t *c =
    pn_amqp_compound_build(
      (pn_amqp_compound_t*)properties, PN_COMPOUND_MAP,
      pni_property_items_vsize, pni_property_items_vemit,
      items);
  va_end(items);
  return (pn_amqp_map_t *)c;
}

bool pn_message_properties_next(pn_amqp_iterator_t *iterator, pn_bytes_t *key, pn_amqp_value_t *val) {
  pni_consumer_t consumer = {.output_start=iterator->start, .size=iterator->size, .position=iterator->position};
  while (true) {
    if (consumer.position>=consumer.size) return false;
    // Ignore map keys that aren't strings as they aren't valid properties
    if (consume_string(&consumer, key)) {
      consume_atom(&consumer, (pn_atom_t*)val);
      iterator->position = consumer.position;
      return true;
    } else {
      consume_anything(&consumer);
    }
  }
}

void pni_fields_items_vemit(pni_emitter_t *emitter, va_list items) {
  pni_compound_context compound = make_compound();
  pn_bytes_t key = va_arg(items, pn_bytes_t);
  while (key.size!=0) {
    pn_amqp_value_t val = va_arg(items, pn_amqp_value_t);
    emit_symbol_bytes(emitter, &compound, key);
    emit_amqp_value(emitter, &compound, &val);
    key = va_arg(items, pn_bytes_t);
  }
}

pn_amqp_map_t *pn_amqp_fields_build(pn_amqp_map_t *annotations, ...) {
  va_list items;
  va_start(items, annotations);
  pn_amqp_compound_t *c =
    pn_amqp_compound_build(
      (pn_amqp_compound_t*)annotations, PN_COMPOUND_MAP,
      pni_property_items_vsize, pni_fields_items_vemit,
      items);
  va_end(items);
  return (pn_amqp_map_t *)c;
}

bool pn_amqp_fields_next(pn_amqp_iterator_t *iterator, pn_bytes_t *key, pn_amqp_value_t *val) {
  pni_consumer_t consumer = {.output_start=iterator->start, .size=iterator->size, .position=iterator->position};
  while (true) {
    if (consumer.position>=consumer.size) return false;
    // Ignore map keys that aren't symbols as they aren't valid fields
    if (consume_symbol(&consumer, key)) {
      consume_amqp_value(&consumer, val);
      iterator->position = consumer.position;
      return true;
    } else {
      consume_anything(&consumer);
    }
  }
}

struct size_count pni_map_items_vsize(va_list items) {
  uint32_t size = 0;
  uint32_t count = 0;

  pn_amqp_value_t key = va_arg(items, pn_amqp_value_t);
  while (key.value_type!=PN_INVALID) {
    pn_amqp_value_t val = va_arg(items, pn_amqp_value_t);

    size += pn_amqp_value_encoded_size(key);
    size += pn_amqp_value_encoded_size(val);
    count += 2;

    key = va_arg(items, pn_amqp_value_t);
  }
  return (struct size_count){size, count};
}

void pni_map_items_vemit(pni_emitter_t *emitter, va_list items) {
  pni_compound_context compound = make_compound();
  pn_amqp_value_t key = va_arg(items, pn_amqp_value_t);
  while (key.value_type!=PN_INVALID) {
    pn_amqp_value_t val = va_arg(items, pn_amqp_value_t);
    emit_amqp_value(emitter, &compound, &key);
    emit_amqp_value(emitter, &compound, &val);
    key = va_arg(items, pn_amqp_value_t);
  }
}

pn_amqp_map_t *pn_amqp_map_build(pn_amqp_map_t *map, ...) {
  va_list items;
  va_start(items, map);
  pn_amqp_compound_t *c =
    pn_amqp_compound_build(
      (pn_amqp_compound_t*)map, PN_COMPOUND_MAP,
      pni_map_items_vsize, pni_map_items_vemit,
      items);
  va_end(items);
  return (pn_amqp_map_t *)c;
}

bool pn_amqp_map_next(pn_amqp_iterator_t *iterator, pn_amqp_value_t *key, pn_amqp_value_t *val) {
  pni_consumer_t consumer = {.output_start=iterator->start, .size=iterator->size, .position=iterator->position};
  if (consumer.position>=consumer.size) return false;
  consume_amqp_value(&consumer, key);
  consume_amqp_value(&consumer, val);
  iterator->position = consumer.position;
  return true;
}

struct size_count pni_list_items_vsize(va_list items) {
  uint32_t size = 0;
  uint32_t count = 0;

  pn_amqp_value_t val = va_arg(items, pn_amqp_value_t);
  while (val.value_type!=PN_INVALID) {
    size += pn_amqp_value_encoded_size(val);
    count += 1;

    val = va_arg(items, pn_amqp_value_t);
  }
  return (struct size_count){size, count};
}

void pni_list_items_vemit(pni_emitter_t *emitter, va_list items) {
  pni_compound_context compound = make_compound();
  pn_amqp_value_t val = va_arg(items, pn_amqp_value_t);
  while (val.value_type!=PN_INVALID) {
    emit_amqp_value(emitter, &compound, &val);
    val = va_arg(items, pn_amqp_value_t);
  }
}

pn_amqp_list_t *pn_amqp_list_build(pn_amqp_list_t *list, ...) {
  va_list items;
  va_start(items, list);
  pn_amqp_compound_t *c =
    pn_amqp_compound_build(
      (pn_amqp_compound_t*)list, PN_COMPOUND_LIST,
      pni_list_items_vsize, pni_list_items_vemit,
      items);
  va_end(items);
  return (pn_amqp_list_t *)c;
}

bool pn_amqp_list_next(pn_amqp_iterator_t *iterator, pn_amqp_value_t *val) {
  pni_consumer_t consumer = {.output_start=iterator->start, .size=iterator->size, .position=iterator->position};
  if (consumer.position>=consumer.size) return false;
  consume_amqp_value(&consumer, val);
  iterator->position = consumer.position;
  return true;
}

struct size_count pni_array_items_vsize(va_list items) {
  uint32_t size = 0;
  uint32_t count = 0;
  unsigned large_encoding = 0;

  pn_bytes_t val = va_arg(items, pn_bytes_t);
  while (val.size!=0) {
    uint32_t item_size = val.size;
    size += item_size;
    count += 1;
    if (item_size>255) large_encoding = 1;

    val = va_arg(items, pn_bytes_t);
  }
  return (struct size_count){1 + count*(1+large_encoding*3) + size, count};
}

void pni_array_items_vemit(pni_emitter_t *emitter, va_list items) {
  // Figure out if small or large symbol encoding
  bool large_encoding = false;
  va_list items2;
  va_copy(items2, items);
  pn_bytes_t val = va_arg(items2, pn_bytes_t);
  while (val.size!=0) {
    uint32_t item_size = val.size;
    if (item_size>255) large_encoding = true;
    val = va_arg(items2, pn_bytes_t);
  }
  va_end(items2);

  if (large_encoding)
    pni_emitter_writef8(emitter, PNE_SYM32);
  else
    pni_emitter_writef8(emitter, PNE_SYM8);

  val = va_arg(items, pn_bytes_t);
  while (val.size!=0) {
    if (large_encoding)
      pni_emitter_writev32(emitter, val);
    else
      pni_emitter_writev8(emitter, val);
    val = va_arg(items, pn_bytes_t);
  }
}

struct size_count pni_array_items_vsize_small(va_list items) {
  uint32_t size = 0;
  uint32_t count = 0;

  pn_bytes_t val = va_arg(items, pn_bytes_t);
  while (val.size!=0) {
    uint32_t item_size = val.size;
    if (item_size>255)
      size += 255;
    else
      size += item_size;
    count += 1;

    val = va_arg(items, pn_bytes_t);
  }
  return (struct size_count){count + size, count};
}

void pni_array_items_vemit_append_small(pni_emitter_t *emitter, va_list items) {
  pn_bytes_t val = va_arg(items, pn_bytes_t);
  while (val.size!=0) {
    if (val.size<256)
      pni_emitter_writev8(emitter, val);
    else
      pni_emitter_writev8(emitter, (pn_bytes_t){.size=255,.start=val.start});
    val = va_arg(items, pn_bytes_t);
  }
}

struct size_count pni_array_items_vsize_large(va_list items) {
  uint32_t size = 0;
  uint32_t count = 0;

  pn_bytes_t val = va_arg(items, pn_bytes_t);
  while (val.size!=0) {
    size += val.size;
    count += 1;

    val = va_arg(items, pn_bytes_t);
  }
  return (struct size_count){count*4 + size, count};
}

void pni_array_items_vemit_append_large(pni_emitter_t *emitter, va_list items) {
  pn_bytes_t val = va_arg(items, pn_bytes_t);
  while (val.size!=0) {
    pni_emitter_writev32(emitter, val);
    val = va_arg(items, pn_bytes_t);
  }
}

pn_amqp_array_t *pn_amqp_symbol_array_build(pn_amqp_array_t *array, ...) {
  va_list items;
  va_start(items, array);
  // We have to do different things if this is the initial build from an append
  // as the initial build needs to put the element typecode there but the append
  // mustn't. Also append cannot change the element typecode so if small then we
  // have to truncate symbols
  pn_amqp_compound_t *c = (pn_amqp_compound_t*)array;
  if (!c) {
    c = pn_amqp_compound_build(
      c, PN_COMPOUND_ARRAY,
      pni_array_items_vsize, pni_array_items_vemit,
      items);
  } else {
    // The element typecode will always be at the same place in the raw buffer
    uint8_t element_type = c->raw[9];
    if (element_type==PNE_SYM32) {
      c = pn_amqp_compound_build(
        c, PN_COMPOUND_ARRAY,
        pni_array_items_vsize_large, pni_array_items_vemit_append_large,
        items);
    } else if (element_type==PNE_SYM8) {
      c = pn_amqp_compound_build(
        c, PN_COMPOUND_ARRAY,
        pni_array_items_vsize_small, pni_array_items_vemit_append_small,
        items);
    }
  }
  va_end(items);
  return (pn_amqp_array_t *)c;
}

pn_amqp_array_t *pn_amqp_symbol_array_buildn(uint32_t count, pn_bytes_t *symbols) {
  if (count==0) return NULL;
  pn_amqp_compound_t *c = NULL;

  unsigned large_encoding = 0;
  uint32_t items_size = 0;

  // Tot up the total size of the symbols and figure out if any are longer than 255
  for (uint32_t i = 0; i < count; i++) {
    uint32_t item_size = symbols[i].size;
    items_size += item_size;
    if (item_size>255) large_encoding = 1;
  }

  // Now add in the correct size for the counts and typecode
  items_size += 1 + count*(1+large_encoding*3);

  c = pni_amqp_compound_new(items_size+9);
  c->raw_size = items_size+9;
  c->items_count = count;
  c->items_size = items_size;
  c->compound_type = PN_COMPOUND_ARRAY;

  // Write header
  bool small_encoding = c->items_size<255 && c->items_count<256;
  if (small_encoding) {
    pn_rwbytes_t bytes = {.size=3, .start=(char*)&c->raw[6]};
    pni_emitter_t emitter = make_emitter_from_bytes(bytes);
    pni_emitter_writef8(&emitter, PNE_ARRAY8);
    // Have to include count in size here
    pni_emitter_writef8(&emitter, c->items_size+1);
    pni_emitter_writef8(&emitter, c->items_count);
  } else {
    pn_rwbytes_t bytes = {.size=9, .start=(char*)&c->raw[0]};
    pni_emitter_t emitter = make_emitter_from_bytes(bytes);
    pni_emitter_writef8(&emitter, PNE_ARRAY32);
    // Have to include count in size here
    pni_emitter_writef32(&emitter, c->items_size+4);
    pni_emitter_writef32(&emitter, c->items_count);
  }

  // Write items
  pn_rwbytes_t bytes = {.size=items_size, .start=(char*)&c->raw[9]};
  pni_emitter_t emitter = make_emitter_from_bytes(bytes);
  if (large_encoding) {
    pni_emitter_writef8(&emitter, PNE_SYM32);
    for (uint32_t i = 0; i < count; i++) {
      pni_emitter_writev32(&emitter, symbols[i]);
    }
  } else {
    pni_emitter_writef8(&emitter, PNE_SYM8);
    for (uint32_t i = 0; i < count; i++) {
      pni_emitter_writev8(&emitter, symbols[i]);
    }
  }

  return (pn_amqp_array_t*) c;
}

bool pn_amqp_symbol_array_next(pn_amqp_iterator_t *iterator, pn_bytes_t *symbol) {
  pni_consumer_t consumer = {.output_start=iterator->start, .size=iterator->size, .position=iterator->position};
  if (consumer.position>=consumer.size) return false;
  uint8_t element_type = iterator->start[0];
  if (element_type==PNE_SYM8) {
    pni_consumer_readv8(&consumer, symbol);
  } else if (element_type==PNE_SYM32) {
    pni_consumer_readv32(&consumer, symbol);
  } else {
    // If it's not a symbol array then set iterator at end and bail
    iterator->position = iterator->size;
    return false;
  }
  iterator->position = consumer.position;
  return true;
}
