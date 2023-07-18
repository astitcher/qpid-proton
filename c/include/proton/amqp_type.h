#ifndef PROTON_AMQP_TYPE_H
#define PROTON_AMQP_TYPE_H 1

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * @copybrief codec
 */

/**
 * Identifies an AMQP type.
 *
 * @ingroup amqp_types
 */
typedef enum {
  /**
   * The NULL AMQP type.
   */
  PN_NULL = 1,

  /**
   * The boolean AMQP type.
   */
  PN_BOOL = 2,

  /**
   * The unsigned byte AMQP type. An 8 bit unsigned integer.
   */
  PN_UBYTE = 3,

  /**
   * The byte AMQP type. An 8 bit signed integer.
   */
  PN_BYTE = 4,

  /**
   * The unsigned short AMQP type. A 16 bit unsigned integer.
   */
  PN_USHORT = 5,

  /**
   * The short AMQP type. A 16 bit signed integer.
   */
  PN_SHORT = 6,

  /**
   * The unsigned int AMQP type. A 32 bit unsigned integer.
   */
  PN_UINT = 7,

  /**
   * The signed int AMQP type. A 32 bit signed integer.
   */
  PN_INT = 8,

  /**
   * The char AMQP type. A 32 bit unicode character.
   */
  PN_CHAR = 9,

  /**
   * The ulong AMQP type. An unsigned 64 bit integer.
   */
  PN_ULONG = 10,

  /**
   * The long AMQP type. A signed 64 bit integer.
   */
  PN_LONG = 11,

  /**
   * The timestamp AMQP type. A signed 64 bit value measuring
   * milliseconds since the epoch.
   */
  PN_TIMESTAMP = 12,

  /**
   * The float AMQP type. A 32 bit floating point value.
   */
  PN_FLOAT = 13,

  /**
   * The double AMQP type. A 64 bit floating point value.
   */
  PN_DOUBLE = 14,

  /**
   * The decimal32 AMQP type. A 32 bit decimal floating point value.
   */
  PN_DECIMAL32 = 15,

  /**
   * The decimal64 AMQP type. A 64 bit decimal floating point value.
   */
  PN_DECIMAL64 = 16,

  /**
   * The decimal128 AMQP type. A 128 bit decimal floating point value.
   */
  PN_DECIMAL128 = 17,

  /**
   * The UUID AMQP type. A 16 byte UUID.
   */
  PN_UUID = 18,

  /**
   * The binary AMQP type. A variable length sequence of bytes.
   */
  PN_BINARY = 19,

  /**
   * The string AMQP type. A variable length sequence of unicode
   * characters.
   */
  PN_STRING = 20,

  /**
   * The symbol AMQP type. A variable length sequence of unicode
   * characters.
   */
  PN_SYMBOL = 21,

  /**
   * A described AMQP type.
   */
  PN_DESCRIBED = 22,

  /**
   * An AMQP array. A monomorphic sequence of other AMQP values.
   */
  PN_ARRAY = 23,

  /**
   *  An AMQP list. A polymorphic sequence of other AMQP values.
   */
  PN_LIST = 24,

  /**
   * An AMQP map. A polymorphic container of other AMQP values formed
   * into key/value pairs.
   */
  PN_MAP = 25,

  /**
   * A special invalid type value that is returned when no valid type
   * is available.
   */
  PN_INVALID = -1
} pn_type_t;

/**
 * Return a string name for an AMQP type.
 *
 * @ingroup amqp_types
 * @param type an AMQP type
 * @return the string name of the given type
 */
PN_EXTERN const char *pn_type_name(pn_type_t type);

#ifdef __cplusplus
}
#endif

#endif // amqp_type.h
