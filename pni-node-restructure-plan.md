# `pni_node_t` Restructure and Encoder Improvement Plan

## Top-Level Overview

`pni_node_t` (the internal node type for `pn_data_t`) carries several members that
are either redundant, misplaced, or never activated. The work falls into four
independent sub-tasks:

1. **Fix the `pn_buffer_ensure` error-return propagation** in `pn_do_transfer` and
   `pn_link_send` — already partially done; captured here for completeness.
2. **Eliminate `pni_data_rebase`** by making `data_offset`/`data_size` the single
   source of truth for interned string data, removing the fragile pointer-patching
   that `pni_data_rebase` performs after every realloc.
3. **Remove `start` and `small` from `pni_node_t`** — `start` moves to a side-stack
   on `pn_encoder_t`; `small` is deleted (its branch is dead code today).
4. **Activate small-encoding** for LIST/MAP/ARRAY using a two-pass approach,
   eliminating the dead `small` branch with a correct implementation.

Sub-tasks 2, 3, and 4 are ordered by dependency: 2 can proceed independently, 3 must
precede 4 (4 replaces what 3 deletes).

---

## Sub-Task 1 — `pn_buffer_append` error propagation (completed)

**Status:** `[x] done`

**Intent:** Ensure OOM failures in `pn_buffer_ensure` are visible to callers rather
than silently continuing with a corrupt buffer state.

**Outcomes:**
- `pn_buffer_ensure` returns `PN_OUT_OF_MEMORY` on realloc failure and does not
  update `buf->capacity` speculatively.
- `pn_do_transfer` calls `pn_do_error` with `"amqp:resource-limit-exceeded"` when
  `pn_buffer_append` fails.
- `pn_link_send` propagates the error code directly as its return value.

**Relevant files:**
- `c/src/core/buffer.c` — `pn_buffer_ensure`
- `c/src/core/transport.c` — `pn_do_transfer` line ~1435
- `c/src/core/engine.c` — `pn_link_send` line ~2349
- `c/src/core/util.h` — `pni_round_up_pow2` helper added here

---

## Sub-Task 2 — Eliminate `pni_data_rebase` / make `data_offset` the single source of truth

**Status:** `[ ] pending`

**Intent:** After every `pn_buffer_append` to `data->buf`, the backing allocation may
move. Currently `pni_data_intern_node` patches all live `node->atom.u.as_bytes.start`
pointers via `pni_data_rebase`. This is fragile and requires a full linear scan of all
nodes on every resize. Switching to offset-based storage means realloc never
invalidates any stored value.

`data_offset` and `data_size` already exist on every node. The goal is to make them
the only representation of interned string location, and derive the pointer on
read-out.

**Expected Outcomes:**
- `pni_data_intern_node` no longer writes `node->atom.u.as_bytes.start` after
  interning; it writes only `node->data_offset` and `node->data_size`.
- `pni_data_rebase` is deleted entirely.
- `pn_data_get_binary`, `pn_data_get_string`, `pn_data_get_symbol`, `pn_data_get_bytes`
  materialise the pointer on the way out: `buf.start + node->data_offset` when
  `node->data == true`.
- `pni_inspect_enter` builds a temporary `pn_atom_t` on the stack with
  `as_bytes.start` materialised before calling `pni_inspect_atom` — signature of
  `pni_inspect_atom` is unchanged.
- `pni_encoder_enter` similarly builds a temporary `pn_bytes_t` with the materialised
  pointer before calling `pn_encoder_writev8/32` — no signature changes.
- `pn_data_get_atom` constructs and returns a `pn_atom_t` by value with the
  materialised pointer for string types.
- All existing tests pass.

**Todo:**
1. In `pni_data_intern_node` (`codec.c:527`): after calling `pni_data_intern`, stop
   writing `bytes->start`; the offset is already in `node->data_offset`. Remove the
   `pn_buffer_capacity` change-detection block and the `pni_data_rebase` call.
2. Delete `pni_data_rebase` (`codec.c:516–524`).
3. Update `pn_data_get_binary`, `pn_data_get_string`, `pn_data_get_symbol`,
   `pn_data_get_bytes` (`codec.c:2127–2170`): when `node->data == true`, return
   `(pn_bytes_t){ node->data_size, pn_buffer_memory(data->buf).start + node->data_offset }`.
4. Update `pn_data_get_atom` (`codec.c:2173`): for string/binary/symbol types with
   `node->data == true`, fill `as_bytes` with the materialised value before returning.
5. Update `pni_inspect_enter` (`codec.c:272`): build a stack-local `pn_atom_t tmp`
   copying `node->atom`, then for `PN_BINARY/STRING/SYMBOL` with `node->data == true`
   overwrite `tmp.u.as_bytes` with the materialised value; pass `&tmp` to
   `pni_inspect_atom`.
6. Update `pni_encoder_enter` (`encoder.c:255`): for the `PNE_VBIN*`, `PNE_STR*`,
   `PNE_SYM*` cases, build a stack-local `pn_bytes_t` with the materialised pointer
   (fetched from `pn_buffer_memory(data->buf).start + node->data_offset`) and pass
   that to `pn_encoder_writev8/32`.
7. Note: `pni_data_intern` (`codec.c:494`) passes `bytes->start` to `pn_buffer_append`
   as the *source* data, before interning — this is always an external pointer and
   is unaffected.

**Relevant files/symbols:**
- `c/src/core/codec.c`: `pni_data_intern_node`, `pni_data_rebase`, `pni_data_bytes`,
  `pn_data_get_binary/string/symbol/bytes`, `pn_data_get_atom`, `pni_inspect_enter`
- `c/src/core/encoder.c`: `pni_encoder_enter`
- `c/src/core/data.h`: `pni_node_t` struct — `data`, `data_offset`, `data_size` fields

---

## Sub-Task 3 — Remove `start` and `small` from `pni_node_t`; add compound stack to encoder

**Status:** `[ ] pending`

**Intent:** `start` and `small` are encoder scratch values stored on nodes only
because the traversal callback mechanism provides no other per-frame context. `start`
must move to a proper side-stack on `pn_encoder_t`. `small` is never set to `true` in
the current code (its activation path in `pni_encoder_exit` is entirely dead), so it
is simply deleted.

**Expected Outcomes:**
- `pni_node_t` no longer contains `start` or `small`.
- `pn_encoder_t` gains a small fixed-size stack of `size_t` positions (`compound_start`)
  and a depth counter (`compound_depth`).
- `pni_encoder_enter` pushes `encoder->position` onto the stack when entering a
  compound; `pni_encoder_exit` pops it.
- The `if (node->small)` branch in `pni_encoder_exit` is deleted; only the 32-bit
  backfill path remains.
- `pni_encoder_enter` no longer writes `node->start` or `node->small`.
- All existing tests pass.

**Todo:**
1. Add `size_t compound_start[64]` and `unsigned compound_depth` to `pn_encoder_t`
   in `encoder.h`. 64 levels is sufficient for any realistic AMQP nesting.
2. Initialise `compound_depth = 0` in `pn_encoder_initialize`.
3. In `pni_encoder_enter` (`encoder.c`): replace `node->start = encoder->position` with
   `encoder->compound_start[encoder->compound_depth++] = encoder->position`. Remove
   all writes to `node->small`.
4. In `pni_encoder_exit` (`encoder.c`): replace `node->start` reads with
   `encoder->compound_start[--encoder->compound_depth]`. Delete the `if (node->small)`
   branch entirely — keep only the `else` (32-bit) body, removing the conditional.
5. Remove `start` and `small` fields from `pni_node_t` in `data.h`.
6. Remove the `node->small = false` writes from the enter function (already covered
   by step 3, but verify no other writes remain).

**Relevant files/symbols:**
- `c/src/core/encoder.h`: `pn_encoder_t` struct
- `c/src/core/encoder.c`: `pni_encoder_enter`, `pni_encoder_exit`,
  `pn_encoder_initialize`
- `c/src/core/data.h`: `pni_node_t` struct

---

## Sub-Task 4 — Activate small-encoding via two-pass approach

**Status:** `[ ] pending` — depends on Sub-Task 3

**Intent:** The AMQP spec defines `LIST8`, `MAP8`, `ARRAY8` encodings for compounds
whose content fits in 255 bytes with a count ≤ 255. Using them saves 6 bytes per
small compound (3 bytes each for size and count fields). The existing single-pass
encoder cannot choose encoding before writing content. A two-pass approach (size pass
already exists as `pn_encoder_size`) can determine sizes bottom-up before the encode
pass writes anything.

The compound side-stack introduced in Sub-Task 3 is extended to carry the pre-computed
content size and count for each compound so the encode pass can select the opcode
before writing the size/count placeholder fields.

**Expected Outcomes:**
- `pn_encoder_encode` runs the size pass first, then the encode pass.
- During the size pass, each compound's encoded content size and child count are
  recorded in a parallel stack in `pn_encoder_t`.
- During the encode pass, `pni_encoder_enter` for LIST/MAP/ARRAY reads the
  pre-computed size and count, selects `*8` or `*32` accordingly, writes the correct
  opcode and correctly-sized fields immediately (no backfill needed).
- `pni_encoder_exit` for LIST/MAP/ARRAY no longer needs to backfill anything for
  compounds that used the small encoding, since the size was known up front. The
  32-bit backfill path is still needed for large compounds since `encoder->position`
  is only known on exit.
- Wire output is valid and decoded correctly by the existing decoder tests.
- All existing tests pass; where applicable, encoded output for small compounds is
  shorter than before.

**Todo:**
1. Add a parallel `size_t compound_size[64]` and `pni_nid_t compound_count[64]`
   arrays to `pn_encoder_t` alongside `compound_start` from Sub-Task 3.
2. Implement a size-traversal pass that, for each compound, records the total encoded
   byte size of its children and its child count into the parallel arrays at the
   matching depth. The existing `pni_encoder_enter`/exit can be split or augmented
   with a size-only mode, or a separate lightweight traversal can be used.
3. In `pni_encoder_enter`, after retrieving the pre-computed `csize` and `ccount` for
   a compound:
   - If `csize ≤ 255 && ccount ≤ 255` (and it is not the zero-length list case):
     write the `*8` opcode, write 1-byte size and count placeholders. Record that
     this level used small encoding (e.g. a `bool compound_small[64]` flag in the
     encoder).
   - Otherwise: write the `*32` opcode and 4-byte placeholders as today.
4. In `pni_encoder_exit`, backfill using the recorded opcode size (1 or 4 bytes)
   for the compound at the current depth.
5. Handle the zero-length list special case: always emit `PNE_LIST0` regardless of
   the pre-computed size (consistent with current behaviour).

**Relevant files/symbols:**
- `c/src/core/encoder.h`: `pn_encoder_t`
- `c/src/core/encoder.c`: `pni_encoder_enter`, `pni_encoder_exit`,
  `pn_encoder_encode`, `pn_encoder_size`
- `c/src/core/data.h`: `pni_node_t` (no changes needed in this sub-task)
- `c/src/core/decoder.c`: existing `PNE_LIST8/MAP8/ARRAY8` handling confirms the
  decoder already accepts small-encoded output

---

## Notes

- Sub-tasks 2 and 3 are independent and can be done in either order.
- Sub-task 4 depends on Sub-task 3 (needs the compound stack).
- At no point does the public `pn_atom_t` / `pn_bytes_t` API change.
- `pni_inspect_atom` signature is unchanged throughout.
- The `node->data` flag remains as the discriminant for whether `data_offset`/
  `data_size` are valid for a given node.
- `data_offset` and `data_size` are currently `size_t` on 64-bit; a follow-on
  consideration (not in scope here) is shrinking them to `uint32_t` since the
  intern buffer is bounded by a `uint16_t` node count and the existing
  `max_buf_size` limit.
