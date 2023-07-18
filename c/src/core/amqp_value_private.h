#ifndef _CORE_AMQP_COMPOUND_PRIVATE_H
#define _CORE_AMQP_COMPOUND_PRIVATE_H 1

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

#include <proton/amqp_value.h>
#include <proton/types.h>

#include <stdarg.h>

#if __cplusplus
extern "C" {
#endif

size_t pn_amqp_value_encoded_size(pn_amqp_value_t v);

typedef struct pn_amqp_compound_t pn_amqp_compound_t;
pn_amqp_compound_t *pn_amqp_compound_make(pn_bytes_t raw);

#if __cplusplus
}
#endif

#endif /* amqp_compound_private.h */
