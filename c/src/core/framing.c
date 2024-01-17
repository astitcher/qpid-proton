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


#include "framing.h"

#include "engine-internal.h"
#include "util.h"

#include <assert.h>

static inline void pn_do_tx_trace(pn_logger_t *logger, uint16_t ch, pn_bytes_t frame)
{
  if (PN_SHOULD_LOG(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME) ) {
    if (frame.size==0) {
      pn_logger_logf(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME, "%u -> (EMPTY FRAME)", ch);
    } else {
      pni_logger_log_msg_frame(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME, frame, "%u -> ", ch);
    }
  }
}

static inline void pn_do_rx_trace(pn_logger_t *logger, uint16_t ch, pn_bytes_t frame)
{
  if (PN_SHOULD_LOG(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME) ) {
    if (frame.size==0) {
      pn_logger_logf(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME, "%u <- (EMPTY FRAME)", ch);
    } else {
      pni_logger_log_msg_frame(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME, frame, "%u <- ", ch);
    }
  }
}

static inline void pn_do_raw_tx_trace(pn_logger_t *logger, pn_buffer_list_t *buffers)
{
  if (PN_SHOULD_LOG(logger, PN_SUBSYSTEM_IO, PN_LEVEL_RAW)) {
    // TODO: Actually do raw tx loggging
    //pni_logger_log_raw(logger, PN_SUBSYSTEM_IO, PN_LEVEL_RAW, frame, size, "->");
  }
}

static inline void pn_do_raw_rx_trace(pn_logger_t *logger, pn_bytes_t frame, size_t size)
{
  if (PN_SHOULD_LOG(logger, PN_SUBSYSTEM_IO, PN_LEVEL_RAW)) {
    pni_logger_log_raw(logger, PN_SUBSYSTEM_IO, PN_LEVEL_RAW, frame, size, "<-");
  }
}

ssize_t pn_read_frame(pn_frame_t *frame, const char *bytes, size_t available, uint32_t max, pn_logger_t *logger)
{
  if (available < AMQP_HEADER_SIZE) return 0;
  uint32_t size = pni_read32(&bytes[0]);
  if (max && size > max) return PN_ERR;
  if (available < size) return 0;
  unsigned int doff = 4 * (uint8_t)bytes[4];
  if (doff < AMQP_HEADER_SIZE || doff > size) return PN_ERR;

  frame->frame_payload0 = pn_buffer_list_entry_from_bytes((pn_bytes_t){.size=size-doff, .start=bytes+doff});
  frame->frame_payload1 = pn_buffer_list_entry_from_bytes((pn_bytes_t){.size=0,.start=NULL});
  frame->extended = pn_buffer_list_entry_from_bytes((pn_bytes_t){.size=doff-AMQP_HEADER_SIZE, .start=bytes+AMQP_HEADER_SIZE});
  frame->type = bytes[5];
  frame->channel = pni_read16(&bytes[6]);

  pn_do_rx_trace(logger, frame->channel, pn_bytes_from_buffer_list_entry(&frame->frame_payload0));
  pn_do_raw_rx_trace(logger, (pn_bytes_t){.size=size, .start=bytes}, AMQP_HEADER_SIZE+frame->extended.size+frame->frame_payload0.size+frame->frame_payload1.size);

  return size;
}

static size_t pn_write_frame(pn_buffer_list_t* buffers, pn_rwbytes_t header_space, pn_frame_t frame, pn_logger_t *logger)
{
  size_t size = AMQP_HEADER_SIZE + frame.extended.size + frame.frame_payload0.size + frame.frame_payload1.size;
  // Prepare header
  char *bytes = header_space.start;
  pni_write32(&bytes[0], size);
  int doff = (frame.extended.size + AMQP_HEADER_SIZE - 1)/4 + 1;
  bytes[4] = doff;
  bytes[5] = frame.type;
  pni_write16(&bytes[6], frame.channel);

  // Write header then rest of frame
  pn_buffer_list_append_head(buffers, pn_buffer_list_entry_from_bytes((pn_bytes_t){.size=8, .start=bytes}));
  if (frame.extended.size) pn_buffer_list_append_head(buffers, frame.extended);

  // Don't mess with the buffer unless we are logging frame traces to avoid
  // shuffling the buffer unnecessarily.
  if (
    PN_SHOULD_LOG(logger, PN_SUBSYSTEM_AMQP, PN_LEVEL_FRAME) ||
    PN_SHOULD_LOG(logger, PN_SUBSYSTEM_IO, PN_LEVEL_RAW))
  {
    // Get current buffer pointer so we can trace dump performative and payload together
    if (frame.frame_payload0.size) pn_buffer_list_append_head(buffers, frame.frame_payload0);
    if (frame.frame_payload1.size) pn_buffer_list_append_head(buffers, frame.frame_payload1);

    pn_do_tx_trace(logger, frame.channel, pn_bytes_from_buffer_list_entry(&frame.frame_payload0));
    pn_do_raw_tx_trace(logger, buffers);
  } else {
    if (frame.frame_payload0.size) pn_buffer_list_append_head(buffers, frame.frame_payload0);
    if (frame.frame_payload1.size) pn_buffer_list_append_head(buffers, frame.frame_payload1);
  }
  return size;
}

static inline size_t pn_post_frame(pn_buffer_list_t *output, pn_logger_t *logger, uint8_t type, uint16_t ch,
                                 pn_rwbytes_t header_space, pn_buffer_list_entry_t performative, pn_buffer_list_entry_t payload)
{
  pn_frame_t frame = {
    .type = type,
    .channel = ch,
    .frame_payload0 = performative,
    .frame_payload1 = payload
  };
  return pn_write_frame(output, header_space, frame, logger);
}

size_t pn_framing_send_heartbeat(pn_transport_t *transport, uint16_t ch)
{
  pn_rwbytes_t header_space = pni_transport_get_header_space(transport);
  size_t size = pn_post_frame(&transport->amqp_buffers, &transport->logger, AMQP_FRAME_TYPE, ch, header_space, pn_buffer_list_entry_null, pn_buffer_list_entry_null);
  transport->output_frames_ct += 1;
  transport->bytes_output += size;
  return size;
}

size_t pn_framing_send_amqp(pn_transport_t *transport, uint16_t ch, pn_buffer_list_entry_t performative)
{
  pn_rwbytes_t header_space = pni_transport_get_header_space(transport);
  size_t size = pn_post_frame(&transport->amqp_buffers, &transport->logger, AMQP_FRAME_TYPE, ch, header_space, performative, pn_buffer_list_entry_null);
  transport->output_frames_ct += 1;
  transport->bytes_output += size;
  return size;
}

size_t pn_framing_send_amqp_with_payload(pn_transport_t *transport, uint16_t ch, pn_buffer_list_entry_t performative, pn_buffer_list_entry_t payload)
{
  pn_rwbytes_t header_space = pni_transport_get_header_space(transport);
  size_t size = pn_post_frame(&transport->amqp_buffers, &transport->logger, AMQP_FRAME_TYPE, ch, header_space, performative, payload);
  transport->output_frames_ct += 1;
  transport->bytes_output += size;
  return size;
}

size_t pn_framing_send_sasl(pn_transport_t *transport, pn_buffer_list_entry_t performative)
{
  // All SASL frames go on channel 0
  pn_rwbytes_t header_space = pni_transport_get_header_space(transport);
  size_t size = pn_post_frame(&transport->amqp_buffers, &transport->logger, SASL_FRAME_TYPE, 0, header_space, performative, pn_buffer_list_entry_null);
  transport->output_frames_ct += 1;
  transport->bytes_output += size;
  return size;
}
