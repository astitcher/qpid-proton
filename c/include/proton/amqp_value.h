#ifndef PROTON_AMQP_VALUE_H
#define PROTON_AMQP_VALUE_H 1

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
#include <proton/amqp_type.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Some extra types that are valid for pn_amqp_value_t that are not valid for pn_atom_t
 *
 * Note that it is important that these values don't overlap with any value for pn_type_t.
 * This is because these values are used to distinguish between different structs which
 * share an initial typecode (this is valid re the C standard which guarantees that the
 * initial member of a struct will be at the same location if it is the same type).
 */
typedef enum {
  PN_COMPOUND_ARRAY = 128,
  PN_COMPOUND_LIST = 129,
  PN_COMPOUND_MAP = 130
} pn_amqp_compund_type_t;

/**
 * Any amqp protocol value.
 *
 * A discriminated union that holds any AMQP value. The type
 * field indicates the AMQP type of the value, and the union may be
 * used to access the value for a given type.
 *
 * This definition is directly compatible with pn_atom_t as used in the pn_data_t
 * code, but this type will not be used outside code that requires pn_data_t.
 *
 * Additionally pn_amqp_value_t can also represent compound AMQP type (list, map and
 * array) with a reference to the raw protocol byte representation.
 *
 * @ingroup amqp_value_types
 */
typedef struct {
  /**
   * Indicates the type of value the atom is currently pointing to.
   * See ::pn_type_t for details on AMQP types.
   */
  pn_type_t value_type;
  // Up to 4 extra bytes can go here and they will always be in necessary padding
  union {
    /**
     * Valid when type is ::PN_BOOL.
     */
    bool as_bool;

    /**
     * Valid when type is ::PN_UBYTE.
     */
    uint8_t as_ubyte;

    /**
     * Valid when type is ::PN_BYTE.
     */
    int8_t as_byte;

    /**
     * Valid when type is ::PN_USHORT.
     */
    uint16_t as_ushort;

    /**
     * Valid when type is ::PN_SHORT.
     */
    int16_t as_short;

    /**
     * Valid when type is ::PN_UINT.
     */
    uint32_t as_uint;

    /**
     * Valid when type is ::PN_INT.
     */
    int32_t as_int;

    /**
     * Valid when type is ::PN_CHAR.
     */
    pn_char_t as_char;

    /**
     * Valid when type is ::PN_ULONG.
     */
    uint64_t as_ulong;

    /**
     * Valid when type is ::PN_LONG.
     */
    int64_t as_long;

    /**
     * Valid when type is ::PN_TIMESTAMP.
     */
    pn_timestamp_t as_timestamp;

    /**
     * Valid when type is ::PN_FLOAT.
     */
    float as_float;

    /**
     * Valid when type is ::PN_DOUBLE.
     */
    double as_double;

    /**
     * Valid when type is ::PN_DECIMAL32.
     */
    pn_decimal32_t as_decimal32;

    /**
     * Valid when type is ::PN_DECIMAL64.
     */
    pn_decimal64_t as_decimal64;

    /**
     * Valid when type is ::PN_DECIMAL128.
     */
    pn_decimal128_t as_decimal128;

    /**
     * Valid when type is ::PN_UUID.
     */
    pn_uuid_t as_uuid;

    /**
     * Valid when type is ::PN_BINARY or ::PN_STRING or ::PN_SYMBOL or
     * ::PN_LIST or ::PN_MAP or PN_ARRRY
     * When the type is ::PN_STRING the field will point to utf8
     * encoded unicode. When the type is ::PN_SYMBOL, the field will
     * point to 7-bit ASCII. In the latter two cases, the bytes
     * pointed to are *not* necessarily null terminated.
     * When the type is ::PN_LIST, ::PN_MAP or ::PN_ARRAY the field will point
     * to the raw encoded bytes for the full embedded compound value (including
     * the typecode)
     * When the type is ::PN_DESCRIBED the field will also point to the raw
     * encoded bytes for the full embedded value.
     */
    pn_bytes_t as_bytes;
  } u;
} pn_amqp_value_t;

/**
 * An iterator over compound amqp values.
 *
 * @ingroup amqp_value_types
 */
struct pn_amqp_iterator_t {
  const uint8_t *start;
  uint32_t size;
  uint32_t position;
};
typedef struct pn_amqp_iterator_t pn_amqp_iterator_t;

/**
 * An amqp map.
 *
 * @ingroup amqp_value_types
 */
typedef struct pn_amqp_map_t pn_amqp_map_t;

/**
 * An amqp list.
 *
 * @ingroup amqp_value_types
 */
typedef struct pn_amqp_list_t pn_amqp_list_t;

/**
 * An amqp array.
 *
 * @ingroup amqp_value_types
 */
typedef struct pn_amqp_array_t pn_amqp_array_t;

typedef struct pn_amqp_string_t pn_amqp_string_t;
typedef struct pn_amqp_symbol_t pn_amqp_symbol_t;

/**
 * A key for an annotations map. (valid types symbol/ulong)
 *
 * @ingroup amqp_value_types
 */
typedef struct pn_amqp_annotations_key_t pn_amqp_annotations_key_t;

static inline pn_type_t pn_amqp_value_type(pn_amqp_value_t *value) {
    return value->value_type;
}

PN_EXTERN pn_bytes_t pn_amqp_value_bytes(pn_amqp_value_t *value);
PN_EXTERN void pn_amqp_value_free(pn_amqp_value_t *value);

PN_EXTERN pn_amqp_map_t *pn_amqp_map_build(pn_amqp_map_t *map, ...);
PN_EXTERN pn_bytes_t pn_amqp_map_bytes(pn_amqp_map_t *map);
PN_EXTERN pn_amqp_iterator_t pn_amqp_map_iterator(pn_amqp_map_t *map);
PN_EXTERN bool pn_amqp_map_next(pn_amqp_iterator_t *iterator, pn_amqp_value_t *key, pn_amqp_value_t *val);
PN_EXTERN void pn_amqp_map_free(pn_amqp_map_t *map);

PN_EXTERN pn_amqp_list_t *pn_amqp_list_build(pn_amqp_list_t *list, ...);
PN_EXTERN pn_bytes_t pn_amqp_list_bytes(pn_amqp_list_t *list);
PN_EXTERN pn_amqp_iterator_t pn_amqp_list_iterator(pn_amqp_list_t *list);
PN_EXTERN bool pn_amqp_list_next(pn_amqp_iterator_t *iterator, pn_amqp_value_t *val);
PN_EXTERN void pn_amqp_list_free(pn_amqp_list_t *list);

PN_EXTERN pn_bytes_t pn_amqp_array_bytes(pn_amqp_array_t *array);
PN_EXTERN pn_amqp_iterator_t pn_amqp_array_iterator(pn_amqp_array_t *array);
PN_EXTERN uint32_t pn_amqp_array_count(pn_amqp_array_t *array);
PN_EXTERN void pn_amqp_array_free(pn_amqp_array_t *array);

PN_EXTERN pn_amqp_array_t *pn_amqp_symbol_array_build(pn_amqp_array_t *array, ...);
PN_EXTERN pn_amqp_array_t *pn_amqp_symbol_array_buildn(uint32_t count, pn_bytes_t *symbols);
PN_EXTERN bool pn_amqp_symbol_array_next(pn_amqp_iterator_t *iterator, pn_bytes_t *symbol);

/*
 * Build a message properties map - the variable args must be pn_bytes_t, pn_atom_t pairs
 * the end of the list is indicated by pn_bytes_null.
 * The pn_bytes_t represents a string key and the pn_atom_t represents a simple amqp value
 */
PN_EXTERN pn_amqp_map_t *pn_message_properties_build(pn_amqp_map_t *properties, ...);
PN_EXTERN bool pn_message_properties_next(pn_amqp_iterator_t *iterator, pn_bytes_t *key, pn_amqp_value_t *val);

PN_EXTERN pn_amqp_map_t *pn_amqp_annotations_build(pn_amqp_map_t *annotations, ...);
PN_EXTERN bool pn_amqp_annotations_next(pn_amqp_iterator_t *iterator, pn_amqp_annotations_key_t *key, pn_amqp_value_t *val);

/*
 * Build a message properties map - the variable args must be pn_bytes_t, pn_amqp_value_t pairs
 * the end of the list is indicated by pn_bytes_null.
 * The pn_bytes_t represents a symbol and the value represents any amqp value
 */
PN_EXTERN pn_amqp_map_t *pn_amqp_fields_build(pn_amqp_map_t *fields, ...);
PN_EXTERN bool pn_amqp_fields_next(pn_amqp_iterator_t *iterator, pn_bytes_t *key, pn_amqp_value_t *val);

#ifdef __cplusplus
}
#endif

#endif /* amqp_value.h */
