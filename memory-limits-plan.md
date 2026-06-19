# Memory Limit Security Plan for Proton-C AMQP Decoding

## Top-Level Overview

This plan addresses unbounded memory consumption vulnerabilities found in the
Proton-C AMQP 1.0 implementation. Two categories of fix are scoped here:

1. **Per-`pn_data_t` instance limits** — bounding how many nodes and how many
   bytes of interned string data a single `pn_data_t` instance may accumulate
   before returning an error, protecting against a malicious peer's raw bytes
   (received during AMQP frame decoding) from inflating into large in-memory
   structures when the application later decodes them via the `pn_data_*` APIs.

2. **Connection-level incomplete multi-frame delivery memory limit** — bounding
   total per-connection bytes buffered across all in-flight multi-frame
   deliveries whose `more` flag has not yet been cleared, preventing a malicious
   sender from occupying unbounded memory by streaming partial messages.

### Architecture note: how `pn_data_t` is used during frame decoding

All AMQP performative field parsing (Open, Begin, Attach, Transfer, Flow,
Disposition, Detach, End, Close) is done by the zero-allocation `pni_consumer_t`
machinery in [`c/src/core/consumers.h`](c/src/core/consumers.h). Fields such as
properties, capabilities, filter, and condition info are captured only as
`pn_bytes_t` slices pointing into the frame buffer — **no `pn_data_t` is
allocated during frame decoding**. A `pn_data_t` is created lazily and on-demand
when the application calls APIs such as
[`pn_connection_remote_properties()`](c/src/core/engine.c:708),
[`pn_terminus_capabilities()`](c/src/core/engine.c:1449),
[`pn_disposition_data()`](c/src/core/engine.c:1893), or
[`pn_condition_info()`](c/src/core/engine.c:2700); all of these call
[`pni_switch_to_data()`](c/src/core/util.h:69) which calls
[`pn_data_decode()`](c/src/core/codec.c:1591).

This means:
- The `pn_data_t` limit (Sub-Task 1) protects against large `pn_data_t`
  instances being created from peer-supplied bytes, but the raw bytes are
  already in memory by then — the limit prevents the secondary inflation from
  decoding, not the raw storage.
- The AMQP frame fields that end up in `pn_data_t` are structurally constrained
  to specific known types (maps of symbol→value for properties, arrays of
  symbols for capabilities, etc.) — a more targeted defence is possible via
  value-type validation at decode time (see Out-of-Scope notes).

### Issues from Issues-to-look-for.md confirmed present

| # | Issue | File / Function | Status |
|---|-------|----------------|--------|
| A | Arrays of 0-width elements causing large node allocation from a tiny frame | `decoder.c` `pni_decoder_decode_value()` lines 401-405 | **Present** — `count` drives a node-allocating loop; 0-width codes (PNE_NULL, PNE_TRUE, PNE_FALSE, PNE_UINT0, PNE_ULONG0) allocate one node each consuming zero bytes. |
| B | Missing recursion depth limit | `decoder.c` `pni_decoder_decode_type()` line 469 | **Present but practically limited** — `pni_allowed_descriptor_code()` at line 480 blocks compound types as descriptors; depth bounded by frame size in practice. |
| C | Unbounded `pn_data_t` buffer growth (string intern) | `codec.c` `pni_data_intern()` lines 453-461 | **Present** — `data->buf` grows without limit as strings/symbols/binaries are interned. |
| D | Unbounded pre-delivery byte growth on multi-frame transfers | `transport.c` `pn_do_transfer()` line 1427 | **Present** — `pn_buffer_append(delivery->bytes, …)` accumulates payload bytes indefinitely while `more=true`. |
| E | Disposition range loop with unbounded iteration | `transport.c` `pn_do_disposition()` lines 1702-1708 | **Present but mitigated** — the hash-iteration branch at line 1693 fires when range > delivery count; sequential branch still exposed for large ranges within a high delivery count. |
| F | Session incoming-window bypass | `transport.c` `pn_do_transfer()` line 1362 | **Partially mitigated** — check fires at window == 0; `pn_session_set_incoming_window_and_lwm` provides proper defence when configured. |
| G | Min-max-frame-size enforcement | `transport.c` `pn_do_open()` lines 1034-1041 | **Fixed** — clamped to `AMQP_MIN_MAX_FRAME_SIZE`. No action needed. |
| H | Unbounded echo flow responses | N/A | **Not applicable** — Proton does not implement echo flow. |

### Scope of this plan

Addresses the two issues called out as definitely present and highest priority:

- **Issue D** — unbounded memory via incomplete multi-frame transfer continuation
  (per-connection bytes in flight).
- **Issue C** — unbounded `pn_data_t` string-intern buffer growth, combined
  with Issue A (0-width array nodes inflating node count).

Issues B, E, F are noted as out-of-scope for this plan but documented.

---

## Sub-Task 1 — Add a per-`pn_data_t` node-count and buffer-size limit

### Intent

`pn_data_t` grows two unbounded resources when populated via `pn_data_decode()`
from peer-supplied bytes:

- An array of `pni_node_t` structs, today soft-capped at `PNI_NID_MAX = 65535`
  (uint16 ceiling, ~5.5 MB on 64-bit at ~84 bytes/node). A peer can force
  allocation of up to 65535 nodes by sending an array of 0-width elements
  (e.g. `PNE_NULL`) where the `size` field is tiny but `count` is near 2^32;
  the existing loop at [`decoder.c:401`](c/src/core/decoder.c:401) will run until
  the uint16 node-count ceiling is hit.
- A string-intern buffer (`data->buf`) that grows without any limit at all as
  strings, symbols, and binary values are interned via
  [`pni_data_intern()`](c/src/core/codec.c:453). A peer can include arbitrarily
  large or numerous strings in properties/capabilities fields.

The fix adds two configurable limit fields to `pn_data_t`. When either limit is
exceeded during a decode, the function returns `PN_OUT_OF_MEMORY`, which
propagates to the caller.

### Limit placement design

The limits are **not** set as defaults on `pn_data()` construction — a freshly
constructed `pn_data_t` is a trusted local object used for encoding outbound
messages, and constraining it would break legitimate application use (e.g. a
binding putting a 100 KB string value into a message body).

Instead, limits are applied **at the point of decode** inside
`pni_switch_to_data()`, which is the exclusive gateway from peer-controlled raw
bytes into a decoded `pn_data_t`. The function takes a `max_nodes` parameter
and calls `pn_data_set_decode_limits()` before `pn_data_decode()`, arming the
limits on every decode even when the same `pn_data_t` is reused across calls.

**Intern buffer limit — `bytes->size` for all call sites:**
The intern buffer limit is always set to `bytes->size` inside
`pni_switch_to_data()` itself — it is not a parameter. Interned string/binary
values must originate from the raw bytes being decoded, so 1:1 is a tight,
correct upper bound that prevents any amplification attack without imposing an
artificial ceiling. For performative fields the raw bytes come from a single
AMQP frame and are already bounded by `remote_max_frame` (≤ 32 KiB by default).
For the message body they are bounded by the transport's
`max_buffered_delivery_bytes` limit.

**Node count limit — varies by call site:**
The node count cannot be bounded by `bytes->size` because 0-width element types
(e.g. `PNE_NULL` in an array) consume a node but zero bytes. Two constants are
defined:

- **`PNI_DATA_DEFAULT_MAX_NODES`** (1024) — all performative fields:
  connection/terminus/link properties and capabilities, condition info,
  disposition data, message annotations and application-properties. These carry
  structured protocol metadata; 1024 nodes is generous for any legitimate value.
- **`PNI_DATA_BODY_MAX_NODES`** (0 = unlimited) — message body only. Application
  data whose node count is bounded only by the uint16 hard ceiling of
  `PNI_NID_MAX` (65535).

The application cannot raise the limits before the first decode fires, because
the `pn_data_t*` fields in all internal structs start as `NULL` and are created
inside `pni_switch_to_data()` on the first accessor call — there is no
pre-existing handle to call `pn_data_set_decode_limits()` on. The limits are
therefore entirely owned by the call sites in `pni_switch_to_data()`.

### ABI/API stability analysis

The `pn_data_t` struct is opaque to callers; its definition is in the internal
[`data.h`](c/src/core/data.h), not in any public header. Adding fields to the
struct is therefore **ABI-safe** — callers never embed it by value or compute
offsets into it.

`pni_switch_to_data()` is an internal `static inline` function; adding
parameters to it is not an API change.

`pn_data_decode()` already returns `ssize_t`, where a negative value signals an
error code (`PN_OUT_OF_MEMORY`, `PN_ARG_ERR`, etc.) and positive values are bytes
consumed. **The existing return type already carries the error** — no API change
is needed for the failure path of `pn_data_decode()` itself.

`pni_switch_to_data()` discards the return value of `pn_data_decode()`. This is
intentional by design — the callers all return `pn_data_t*`, not an error code,
and cannot propagate an error up-stack. When the limit is hit:

- `pn_data_decode()` returns `PN_OUT_OF_MEMORY` and sets the error on
  `data->error` (via the existing [`pni_decoder_error()`](c/src/core/decoder.c:29)
  path).
- The returned `pn_data_t*` points to a partially-decoded or empty `pn_data_t`
  with an error set on it.
- The caller can inspect the error via `pn_data_errno()` or `pn_data_error()`,
  both existing stable public APIs.

The only new public API added is the setter:
`pn_data_set_decode_limits(pn_data_t*, size_t max_nodes, size_t max_buf)`.
This is a pure addition and does not alter any existing signature or behaviour.

### Implemented changes

- `struct pn_data_t` in [`data.h`](c/src/core/data.h) has two fields:
  `max_nid` (`pni_nid_t`) and `max_buf_size` (`size_t`), both `0` (unlimited)
  by default.
- Two constants in [`data.h`](c/src/core/data.h):
  `PNI_DATA_DEFAULT_MAX_NODES` (1024, performative fields) and
  `PNI_DATA_BODY_MAX_NODES` (0, message body). No buf-size constant exists;
  the limit is always `bytes->size` at runtime.
- [`pni_data_grow()`](c/src/core/codec.c:474) enforces `max_nid` before
  reallocating.
- [`pni_data_intern()`](c/src/core/codec.c:494) enforces `max_buf_size` before
  appending.
- Both return `PN_OUT_OF_MEMORY` when their limit is exceeded; this propagates
  through the decoder back to `pn_data_decode()` as a negative `ssize_t`.
- `pn_data()` initialises both fields to `0` (unlimited).
- `pni_switch_to_data()` in [`util.h`](c/src/core/util.h) takes a `max_nodes`
  parameter and calls `pn_data_set_decode_limits(*data, max_nodes, bytes->size)`
  before each decode.
- All 16 engine/disposition call sites pass `PNI_DATA_DEFAULT_MAX_NODES`.
- `pn_message_{instructions,annotations,properties}` pass `PNI_DATA_DEFAULT_MAX_NODES`.
- `pn_message_body()` passes `PNI_DATA_BODY_MAX_NODES`.
- A new public setter `pn_data_set_decode_limits(pn_data_t*, size_t max_nodes,
  size_t max_buf)` is in `codec.c` and declared in
  [`c/include/proton/codec.h`](c/include/proton/codec.h).
- `pn_data_clear()` does **not** reset the limits.

### Todo List

1. ~~Add `max_nodes` and `max_buf` fields to `struct pn_data_t`.~~ ✓
2. ~~Initialise both to 0 (unlimited) in `pn_data()`.~~ ✓
3. ~~Enforce `max_nid` in `pni_data_grow()`.~~ ✓
4. ~~Enforce `max_buf_size` in `pni_data_intern()`.~~ ✓
5. ~~Add `pn_data_set_decode_limits()` public setter.~~ ✓
6. ~~Extend `pni_switch_to_data()` with limit parameters; arm limits at all
   call sites with appropriate constants.~~ ✓
7. Add tests to [`c/tests/`](c/tests/) verifying:
   - Performative-field limits are enforced when decoding data that exceeds them.
   - `pn_data_decode()` returns a negative error code when the limit is hit.
   - `pn_data_errno()` reports `PN_OUT_OF_MEMORY` after a limit-exceeded decode.
   - `pn_data_set_decode_limits()` can override the limits.
   - `pn_data_clear()` does not reset the limits.
   - Message body decoding is not blocked by the performative-field limits.
   - User-created `pn_data_t` (via `pn_data()`) is unlimited by default.

### Status

[x] complete — builds clean; ruby binding tests pass

---

## Sub-Task 2 — Limit unread link-buffer bytes per connection

### Intent

The memory concern is not the total size of a streaming message — that can
legitimately be unbounded — but the bytes that have been received by the
transport and placed into `delivery->bytes` buffers but not yet removed by the
application via [`pn_link_recv()`](c/src/core/engine.c:2358) or
[`pn_link_advance()`](c/src/core/engine.c:2225).

There is already a `ssn->incoming_bytes` counter at session level that tracks
exactly this: incremented in [`pn_do_transfer()`](c/src/core/transport.c:1459)
by `payload.size`, decremented in [`pn_link_recv()`](c/src/core/engine.c:2368)
by bytes actually read, and decremented in
[`pni_advance_receiver()`](c/src/core/engine.c:2215) by bytes dropped on
`pn_link_advance()`. This counter already drives the session incoming-window
mechanism when the application configures it via
[`pn_session_set_incoming_capacity()`](c/src/core/engine.c:1131) or
[`pn_session_set_incoming_window_and_lwm()`](c/src/core/engine.c:1147).

The gap is: **when session flow control is not configured, there is no cap on
how many bytes can accumulate in the sum of all `delivery->bytes` buffers on a
connection.** Both single-frame and multi-frame deliveries accumulate there
until the application reads or discards them. A sender that has been granted
link credit can stream frames indefinitely, filling these buffers without bound.

The fix adds a per-transport limit on total unread delivery-buffer bytes,
aggregated across all sessions on the connection. The transport-level counter
mirrors the per-session `ssn->incoming_bytes` counters and is maintained in
exactly the same three places they are. This is the right granularity for a
process-wide memory guard.

**A non-zero default is required.** The C++ binding ([`connection_options.hpp`](cpp/include/proton/connection_options.hpp))
and Python binding ([`_transport.py`](python/proton/_transport.py)) expose no
way to set this limit today — neither has a `max_buffered_delivery_bytes`
option. Most Proton users reach the C layer only through one of these two
bindings. If the limit defaults to 0 (unlimited), it will provide no protection
at all to C++ and Python users without additional binding-layer work. A
conservative non-zero default in the C transport initialiser is the only
practical protection for the vast majority of users.

A suitable default is **~4 MiB** (e.g. `4 * 1024 * 1024` bytes). This is:
- Large enough not to interfere with any normal single-frame or moderately
  sized multi-frame message workload.
- Small enough to prevent runaway memory consumption from a malicious or
  misbehaving peer.
- Consistent in scale with `PN_DEFAULT_MAX_FRAME_SIZE` (32 KiB), being
  roughly 128 frames worth of data.

Applications that legitimately need more (e.g. streaming pipelines with
very large messages that buffer many frames before the application reads them)
can raise the limit via the new C API, or via future additions to the C++ and
Python binding option objects.

### ABI/API stability and error path analysis

**ABI stability:** `pn_transport_t` is also an opaque struct; its definition is
in the internal [`engine-internal.h`](c/src/core/engine-internal.h). Adding two
`size_t` fields is **ABI-safe**.

**Error path when the limit is exceeded in `pn_do_transfer()`:**

`pn_do_transfer()` is a frame-handler called from the frame dispatcher
([`dispatcher.c:100`](c/src/core/dispatcher.c:100)), which is called from
`pn_input_read_amqp()` ([`transport.c:2622`](c/src/core/transport.c:2622)).
The error path from a frame handler is:

1. The handler calls [`pn_do_error(transport, condition, fmt, ...)`](c/src/core/transport.c:954)
   which:
   - Sets the condition on `transport->condition`.
   - Posts a `PN_TRANSPORT_ERROR` event to the collector.
   - Calls each layer's `handle_error` callback — the AMQP layer's
     [`pn_error_amqp()`](c/src/core/transport.c:2558) sends an AMQP Close frame
     carrying the condition, sets `transport->halt = true` and
     `transport->done_processing = true`.
   - Calls `pni_close_tail()` which posts `PN_TRANSPORT_TAIL_CLOSED` and
     potentially `PN_TRANSPORT_CLOSED`.
   - Returns `PN_ERR`.
2. The handler returns `PN_ERR` to `pni_dispatch_frame()`, which returns it to
   `pn_dispatcher_input()` ([`dispatcher.c:118`](c/src/core/dispatcher.c:118)).
3. `pn_input_read_amqp()` sees `n < 0` and returns `PN_EOS`
   ([`transport.c:2623`](c/src/core/transport.c:2623)), halting further input.

The result is a clean AMQP-level connection close with an appropriate condition
(`"amqp:resource-limit-exceeded"` is the most semantically precise condition
name for this situation — it is already used in Proton for the idle-timeout
case at [`transport.c:2645`](c/src/core/transport.c:2645)). The peer receives
a Close frame, the connection is torn down, and the application sees a
`PN_TRANSPORT_ERROR` event followed by `PN_TRANSPORT_CLOSED`.

**Decrement path in `pn_link_recv()` and `pni_advance_receiver()`:**

These are called from application code (outside the frame-reading loop), so
they cannot call `pn_do_error()` — the transport may not even be in a processing
context. The counter is only ever decremented here, so it cannot exceed the
limit; it just needs to be decremented atomically with `ssn->incoming_bytes`.
No error handling is needed at the decrement sites.

**Null transport guard:** In `pn_link_recv()` and `pni_advance_receiver()` the
transport is reachable as `link->session->connection->transport`. This pointer
can be NULL if the connection has been unbound from the transport. The decrement
should be guarded: `if (transport) transport->buffered_delivery_bytes -= ...`.

### Expected Outcomes

- `struct pn_transport_t` in
  [`engine-internal.h`](c/src/core/engine-internal.h) gains:
  - `max_buffered_delivery_bytes` (`size_t`, default `4 * 1024 * 1024`)
  - `buffered_delivery_bytes` (`size_t`, running counter, default 0)
- In [`pn_do_transfer()`](c/src/core/transport.c), alongside the existing
  `ssn->incoming_bytes += payload.size` at line 1459:
  - `transport->buffered_delivery_bytes += payload.size`
  - **Before** the `pn_buffer_append` at line 1427: check limit and call
     `pn_do_error()` with `"amqp:resource-limit-exceeded"` if exceeded.
- In [`pn_link_recv()`](c/src/core/engine.c:2368), alongside
  `ssn->incoming_bytes -= size`:
  - `transport->buffered_delivery_bytes -= size`
- In [`pni_advance_receiver()`](c/src/core/engine.c:2215), alongside
  `ssn->incoming_bytes -= drop_count`:
  - `transport->buffered_delivery_bytes -= drop_count`
- New public APIs `pn_transport_set_max_buffered_delivery_bytes()` and
  `pn_transport_get_max_buffered_delivery_bytes()` added to
  [`transport.c`](c/src/core/transport.c) and declared in
  [`c/include/proton/transport.h`](c/include/proton/transport.h).
- Default behaviour protects all users including C++ and Python binding users
  without any configuration.
- The limit can be raised to 0 (unlimited) or any larger value via the new C API.
- Tests verify:
  - Transfers are blocked when the default limit is reached.
  - Explicitly raising the limit allows more transfers.
  - Setting the limit to 0 disables it entirely.
  - Reading via `pn_link_recv()` reduces the counter and allows further transfers.
  - Advancing via `pn_link_advance()` likewise reduces the counter.
- **Future work (out of scope for this plan):** The C++ [`connection_options`](cpp/include/proton/connection_options.hpp)
  and Python [`Transport`](python/proton/_transport.py) should gain a corresponding
  option so users of those bindings can adjust the limit without dropping to the
  raw C API.

### Todo List

1. Add `max_buffered_delivery_bytes` (`size_t`) and `buffered_delivery_bytes`
   (`size_t`) to `struct pn_transport_t` in
   [`c/src/core/engine-internal.h`](c/src/core/engine-internal.h:132).
   Initialise `buffered_delivery_bytes` to 0 and `max_buffered_delivery_bytes`
   to a named constant `PN_DEFAULT_MAX_BUFFERED_DELIVERY_BYTES` defined as
   `(4 * 1024 * 1024)`, placed alongside `PN_DEFAULT_MAX_FRAME_SIZE` in
   [`engine-internal.h`](c/src/core/engine-internal.h:144).
2. In [`pn_do_transfer()`](c/src/core/transport.c) **before** the
   `pn_buffer_append(delivery->bytes, ...)` at line 1427:
   - Check: `transport->max_buffered_delivery_bytes > 0 &&
     transport->buffered_delivery_bytes + payload.size >
     transport->max_buffered_delivery_bytes` → call `pn_do_error()` with
     `"amqp:resource-limit-exceeded"` and return the error.
3. At line 1459 where `ssn->incoming_bytes += payload.size` is done, add:
   `transport->buffered_delivery_bytes += payload.size`.
4. In [`pn_link_recv()`](c/src/core/engine.c:2368) where
   `ssn->incoming_bytes -= size` is done, add a null-guarded decrement:
   `pn_transport_t *t = receiver->session->connection->transport;
   if (t) t->buffered_delivery_bytes -= size;`
5. In [`pni_advance_receiver()`](c/src/core/engine.c:2215) where
   `ssn->incoming_bytes -= drop_count` is done, add a null-guarded decrement
   via `link->session->connection->transport`.
6. Add public getter/setter in [`transport.c`](c/src/core/transport.c) and
   declarations in [`c/include/proton/transport.h`](c/include/proton/transport.h).
7. Add tests.

### Relevant Context

- [`c/src/core/transport.c:1426-1461`](c/src/core/transport.c:1426) —
  `pn_do_transfer()`: append at line 1427 (guard here), increment at line 1459
  (mirror here).
- [`c/src/core/engine.c:2358-2377`](c/src/core/engine.c:2358) —
  `pn_link_recv()`: `ssn->incoming_bytes -= size` at line 2368 (mirror here).
- [`c/src/core/engine.c:2203-2222`](c/src/core/engine.c:2203) —
  `pni_advance_receiver()`: `ssn->incoming_bytes -= drop_count` at line 2215
  (mirror here).
- [`c/src/core/engine-internal.h:132-226`](c/src/core/engine-internal.h:132) —
  `struct pn_transport_t`.
- No per-delivery tracking field is needed. The transport counter mirrors the
  existing `ssn->incoming_bytes` accounting exactly — the three maintenance
  sites are the only places that counter changes.
- Transport pointer can be NULL after connection unbound from transport; the
  decrement sites in `pn_link_recv()` and `pni_advance_receiver()` must null-guard.
  The increment site in `pn_do_transfer()` is always within a live transport
  context so no guard is needed there.
- The limit guard must be placed **before** `pn_buffer_append` but **after**
  the delivery/session lookup, so the error path has full context.
- The correct AMQP condition is `"amqp:resource-limit-exceeded"`, consistent
  with its use for the idle-timeout case at [`transport.c:2645`](c/src/core/transport.c:2645).

### Status

[ ] pending

---

## Notes on Out-of-Scope Issues

The following issues from `Issues-to-look-for.md` were confirmed present but
are **not** addressed in this plan:

- **Issue A (0-width array nodes)**: Partially mitigated by the `pn_data_t`
  node-count limit in Sub-Task 1. A more targeted fix at the decoder level would
  cap `count` against `(available_bytes / minimum_element_size)` for the
  specific 0-width type codes in the array loop at
  [`decoder.c:401`](c/src/core/decoder.c:401), but this is a separate change.

- **Issue B (recursion depth in `pni_decoder_decode_type`)**: Practically
  limited by `pni_allowed_descriptor_code()` blocking compound descriptor types,
  and by the frame size limiting available bytes. A depth counter would still
  be a belt-and-braces improvement.

- **Issue E (disposition range loop)**: The hash-iteration optimisation at
  [`transport.c:1693`](c/src/core/transport.c:1693) already handles the case
  where the arithmetic range exceeds the delivery count. The remaining exposure
  (large arithmetic range, even larger delivery count) could be closed by
  clamping `last - first` to a configurable maximum.

- **Issue F (session incoming-window bypass)**: The existing window check is
  sound; `pn_session_set_incoming_window_and_lwm` provides proper defence when
  configured by the application.

- **`pn_data_t` field-type validation**: Because fields decoded from AMQP frames
  into `pn_data_t` (properties, capabilities, filter, condition info) have
  well-specified types in the spec (e.g. properties is always a `map<symbol,*>`,
  capabilities is always `array<symbol>`), an alternative or complementary
  approach to the generic size limit in Sub-Task 1 would be to validate the
  type structure at decode time and reject malformed input early. This would
  give more precise errors and could catch type-confusion attacks, but requires
  per-field knowledge and is a larger, separate effort.
