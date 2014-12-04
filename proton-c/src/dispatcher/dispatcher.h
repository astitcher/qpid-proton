#ifndef _PROTON_DISPATCHER_H
#define _PROTON_DISPATCHER_H 1

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

#include <sys/types.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#include "proton/transport.h"
#include "buffer.h"

typedef struct pn_dispatcher_t pn_dispatcher_t;

typedef int (pn_action_t)(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload);

struct pn_dispatcher_t {
  pn_transport_t *transport; // TODO: We keep this to get access to logging - perhaps move logging
  bool halt;
};

pn_dispatcher_t *pn_dispatcher(pn_transport_t *transport);
void pn_dispatcher_free(pn_dispatcher_t *disp);
int pn_post_frame(pn_transport_t *transport, uint8_t type, uint16_t ch, const char *fmt, ...);
ssize_t pn_dispatcher_input(pn_dispatcher_t *disp, const char *bytes, size_t available, bool batch);
ssize_t pn_dispatcher_output(pn_transport_t *transport, char *bytes, size_t size);
int pn_post_amqp_transfer_frame(pn_transport_t *transport,
                                uint16_t local_channel,
                                uint32_t handle,
                                pn_sequence_t delivery_id,
                                pn_bytes_t *payload,
                                const pn_bytes_t *delivery_tag,
                                uint32_t message_format,
                                bool settled,
                                bool more,
                                pn_sequence_t frame_limit);
#endif /* dispatcher.h */
