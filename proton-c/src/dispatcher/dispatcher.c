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

#include "framing/framing.h"
#include "dispatcher.h"
#include "protocol.h"
#include "util.h"
#include "platform_fmt.h"
#include "engine/engine-internal.h"

#include "dispatch_actions.h"

int pni_bad_frame(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload) {
  pn_transport_logf(transport, "Error dispatching frame: type: %d: Unknown performative", frame_type);
  return PN_ERR;
}

int pni_bad_frame_type(pn_transport_t *transport, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload) {
    pn_transport_logf(transport, "Error dispatching frame: Unknown frame type: %d", frame_type);
    return PN_ERR;
}

// We could use a table based approach here if we needed to dynamically
// add new performatives
static inline int pni_dispatch_action(pn_transport_t* transport, uint64_t lcode, uint8_t frame_type, uint16_t channel, pn_data_t *args, const pn_bytes_t *payload)
{
  pn_action_t *action;
  switch (frame_type) {
  case AMQP_FRAME_TYPE:
    /* Regular AMQP fames */
    switch (lcode) {
    case OPEN:            action = pn_do_open; break;
    case BEGIN:           action = pn_do_begin; break;
    case ATTACH:          action = pn_do_attach; break;
    case FLOW:            action = pn_do_flow; break;
    case TRANSFER:        action = pn_do_transfer; break;
    case DISPOSITION:     action = pn_do_disposition; break;
    case DETACH:          action = pn_do_detach; break;
    case END:             action = pn_do_end; break;
    case CLOSE:           action = pn_do_close; break;
    default:              action = pni_bad_frame; break;
    };
    break;
  case SASL_FRAME_TYPE:
    /* SASL frames */
    switch (lcode) {
    case SASL_MECHANISMS: action = pn_do_mechanisms; break;
    case SASL_INIT:       action = pn_do_init; break;
    case SASL_CHALLENGE:  action = pn_do_challenge; break;
    case SASL_RESPONSE:   action = pn_do_response; break;
    case SASL_OUTCOME:    action = pn_do_outcome; break;
    default:              action = pni_bad_frame; break;
    };
    break;
  default:              action = pni_bad_frame_type; break;
  };
  return action(transport, frame_type, channel, args, payload);
}

pn_dispatcher_t *pn_dispatcher(pn_transport_t *transport)
{
  pn_dispatcher_t *disp = (pn_dispatcher_t *) calloc(sizeof(pn_dispatcher_t), 1);

  disp->transport = transport;
  disp->halt = false;

  return disp;
}

void pn_dispatcher_free(pn_dispatcher_t *disp)
{
  free(disp);
}

typedef enum {IN, OUT} pn_dir_t;

static void pn_do_trace(pn_transport_t *transport, uint16_t ch, pn_dir_t dir,
                        pn_data_t *args, const char *payload, size_t size)
{
  if (transport->trace & PN_TRACE_FRM) {
    pn_string_format(transport->scratch, "%u %s ", ch, dir == OUT ? "->" : "<-");
    pn_inspect(args, transport->scratch);

    if (pn_data_size(args)==0) {
        pn_string_addf(transport->scratch, "(EMPTY FRAME)");
    }

    if (size) {
      char buf[1024];
      int e = pn_quote_data(buf, 1024, payload, size);
      pn_string_addf(transport->scratch, " (%" PN_ZU ") \"%s\"%s", size, buf,
                     e == PN_OVERFLOW ? "... (truncated)" : "");
    }

    pn_transport_log(transport, pn_string_get(transport->scratch));
  }
}

static int pni_dispatch_frame(pn_transport_t * transport, pn_data_t *args, pn_frame_t frame)
{
  if (frame.size == 0) { // ignore null frames
    if (transport->trace & PN_TRACE_FRM)
      pn_transport_logf(transport, "%u <- (EMPTY FRAME)\n", frame.channel);
    return 0;
  }

  ssize_t dsize = pn_data_decode(args, frame.payload, frame.size);
  if (dsize < 0) {
    pn_string_format(transport->scratch,
                     "Error decoding frame: %s %s\n", pn_code(dsize),
                     pn_error_text(pn_data_error(args)));
    pn_quote(transport->scratch, frame.payload, frame.size);
    pn_transport_log(transport, pn_string_get(transport->scratch));
    return dsize;
  }

  uint8_t frame_type = frame.type;
  uint16_t channel = frame.channel;
  // XXX: assuming numeric -
  // if we get a symbol we should map it to the numeric value and dispatch on that
  uint64_t lcode;
  bool scanned;
  int e = pn_data_scan(args, "D?L.", &scanned, &lcode);
  if (e) {
    pn_transport_log(transport, "Scan error");
    return e;
  }
  if (!scanned) {
    pn_transport_log(transport, "Error dispatching frame");
    return PN_ERR;
  }
  size_t payload_size = frame.size - dsize;
  const char *payload_mem = payload_size ? frame.payload + dsize : NULL;
  pn_bytes_t payload = {payload_size, payload_mem};

  pn_do_trace(transport, channel, IN, args, payload_mem, payload_size);

  int err = pni_dispatch_action(transport, lcode, frame_type, channel, args, &payload);

  pn_data_clear(args);

  return err;
}

ssize_t pn_dispatcher_input(pn_dispatcher_t *disp, const char *bytes, size_t available, bool batch)
{
  size_t read = 0;
  pn_transport_t *transport = disp->transport;

  while (available && !disp->halt) {
    pn_frame_t frame;

    size_t n = pn_read_frame(&frame, bytes + read, available);
    if (n) {
      read += n;
      available -= n;
      transport->input_frames_ct += 1;
      int e = pni_dispatch_frame(transport, transport->args, frame);
      if (e) return e;
    } else {
      break;
    }

    if (!batch) break;
  }

  return read;
}

int pn_post_frame(pn_transport_t *transport, uint8_t type, uint16_t ch, const char *fmt, ...)
{
  pn_buffer_t *frame_buf = transport->frame;
  va_list ap;
  va_start(ap, fmt);
  pn_data_clear(transport->output_args);
  int err = pn_data_vfill(transport->output_args, fmt, ap);
  va_end(ap);
  if (err) {
    pn_transport_logf(transport,
                      "error posting frame: %s, %s: %s", fmt, pn_code(err),
                      pn_error_text(pn_data_error(transport->output_args)));
    return PN_ERR;
  }

  pn_do_trace(transport, ch, OUT, transport->output_args, NULL, 0);

 encode_performatives:
  pn_buffer_clear( frame_buf );
  pn_buffer_memory_t buf = pn_buffer_memory( frame_buf );
  buf.size = pn_buffer_available( frame_buf );

  ssize_t wr = pn_data_encode( transport->output_args, buf.start, buf.size );
  if (wr < 0) {
    if (wr == PN_OVERFLOW) {
      pn_buffer_ensure( frame_buf, pn_buffer_available( frame_buf ) * 2 );
      goto encode_performatives;
    }
    pn_transport_logf(transport,
                      "error posting frame: %s", pn_code(wr));
    return PN_ERR;
  }

  pn_frame_t frame = {type};
  frame.channel = ch;
  frame.payload = buf.start;
  frame.size = wr;
  size_t n;
  while (!(n = pn_write_frame(transport->output + transport->available,
                              transport->capacity - transport->available, frame))) {
    transport->capacity *= 2;
    transport->output = (char *) realloc(transport->output, transport->capacity);
  }
  transport->output_frames_ct += 1;
  if (transport->trace & PN_TRACE_RAW) {
    pn_string_set(transport->scratch, "RAW: \"");
    pn_quote(transport->scratch, transport->output + transport->available, n);
    pn_string_addf(transport->scratch, "\"");
    pn_transport_log(transport, pn_string_get(transport->scratch));
  }
  transport->available += n;

  return 0;
}

ssize_t pn_dispatcher_output(pn_transport_t *transport, char *bytes, size_t size)
{
  int n = transport->available < size ? transport->available : size;
  memmove(bytes, transport->output, n);
  memmove(transport->output, transport->output + n, transport->available - n);
  transport->available -= n;
  // XXX: need to check for errors
  return n;
}


int pn_post_amqp_transfer_frame(pn_transport_t *transport, uint16_t ch,
                                uint32_t handle,
                                pn_sequence_t id,
                                pn_bytes_t *payload,
                                const pn_bytes_t *tag,
                                uint32_t message_format,
                                bool settled,
                                bool more,
                                pn_sequence_t frame_limit)
{
  bool more_flag = more;
  int framecount = 0;
  pn_buffer_t *frame = transport->frame;

  // create preformatives, assuming 'more' flag need not change

 compute_performatives:
  pn_data_clear(transport->output_args);
  int err = pn_data_fill(transport->output_args, "DL[IIzIoo]", TRANSFER,
                         handle, id, tag->size, tag->start,
                         message_format,
                         settled, more_flag);
  if (err) {
    pn_transport_logf(transport,
                      "error posting transfer frame: %s: %s", pn_code(err),
                      pn_error_text(pn_data_error(transport->output_args)));
    return PN_ERR;
  }

  do { // send as many frames as possible without changing the 'more' flag...

  encode_performatives:
    pn_buffer_clear( frame );
    pn_buffer_memory_t buf = pn_buffer_memory( frame );
    buf.size = pn_buffer_available( frame );

    ssize_t wr = pn_data_encode(transport->output_args, buf.start, buf.size);
    if (wr < 0) {
      if (wr == PN_OVERFLOW) {
        pn_buffer_ensure( frame, pn_buffer_available( frame ) * 2 );
        goto encode_performatives;
      }
      pn_transport_logf(transport, "error posting frame: %s", pn_code(wr));
      return PN_ERR;
    }
    buf.size = wr;

    // check if we need to break up the outbound frame
    size_t available = payload->size;
    if (transport->remote_max_frame) {
      if ((available + buf.size) > transport->remote_max_frame - 8) {
        available = transport->remote_max_frame - 8 - buf.size;
        if (more_flag == false) {
          more_flag = true;
          goto compute_performatives;  // deal with flag change
        }
      } else if (more_flag == true && more == false) {
        // caller has no more, and this is the last frame
        more_flag = false;
        goto compute_performatives;
      }
    }

    if (pn_buffer_available( frame ) < (available + buf.size)) {
      // not enough room for payload - try again...
      pn_buffer_ensure( frame, available + buf.size );
      goto encode_performatives;
    }

    pn_do_trace(transport, ch, OUT, transport->output_args, payload->start, available);

    memmove( buf.start + buf.size, payload->start, available);
    payload->start += available;
    payload->size -= available;
    buf.size += available;

    pn_frame_t frame = {AMQP_FRAME_TYPE};
    frame.channel = ch;
    frame.payload = buf.start;
    frame.size = buf.size;

    size_t n;
    while (!(n = pn_write_frame(transport->output + transport->available,
                                transport->capacity - transport->available, frame))) {
      transport->capacity *= 2;
      transport->output = (char *) realloc(transport->output, transport->capacity);
    }
    transport->output_frames_ct += 1;
    framecount++;
    if (transport->trace & PN_TRACE_RAW) {
      pn_string_set(transport->scratch, "RAW: \"");
      pn_quote(transport->scratch, transport->output + transport->available, n);
      pn_string_addf(transport->scratch, "\"");
      pn_transport_log(transport, pn_string_get(transport->scratch));
    }
    transport->available += n;
  } while (payload->size > 0 && framecount < frame_limit);

  return framecount;
}
