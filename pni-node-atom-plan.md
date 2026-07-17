# `pni_node_t` — Replace `atom` with a compact node-value union

## Top-Level Overview

`pni_node_t` currently carries a `pn_atom_t atom` plus two separate fields
(`data_offset`, `data_size`) that are only ever used for `PN_BINARY`,
`PN_STRING`, and `PN_SYMBOL` nodes. The goal is to replace all of these with a
purpose-built `pni_node_payload_t` union and a small set of direct fields:

- The union members cover only the types actually stored in nodes.
- `PN_BINARY`/`PN_STRING`/`PN_SYMBOL`/`PN_DECIMAL128`/`PN_UUID` nodes use a
  `{ uint32_t offset; uint32_t size; }` arm inside the union. All five types
  are interned into `data->buf`; the 16-byte opaque types (decimal128, uuid)
  use the same path as strings. This drops the union's maximum member from 16
  bytes to 8 bytes (`double`/`uint64_t`).
- `PN_ARRAY` element type lives in a union arm `array_type`, active only for
  array nodes.
- The `data` bool flag (which recorded whether `data_offset` was valid) is
  eliminated — the union arm is either active or not, based on the type tag.
- `type` is the first field on `pni_node_t`. `described` and `small` follow
  immediately, filling the padding gap before the 8-byte-aligned union. `start`
  moves after the union so the hot path fields are front-of-struct.

### Padding analysis (64-bit)

`pn_type_t` is 4 bytes. The union `u` requires 8-byte alignment. That leaves a
4-byte gap between `type` and `u`. Filling it:

| Option | Wastes |
|---|---|
| `array_type` (4 bytes, one field) | 0 |
| `described` + `small` (2 bytes) | 2 bytes internal padding |
| `described` + `small` + one `pni_nid_t` (2+2 = 4 bytes) | 0 |

The cleanest zero-waste option that also reduces the trailing-bool padding is
to put `described`, `small`, and one `pni_nid_t` (e.g. `next`) in the gap.
`array_type` then moves into the union where it only costs space on PN_ARRAY
nodes conceptually (though the union is always 8 bytes regardless).

### Size reduction (64-bit)

| | Current | After |
|---|---|---|
| `start` (size_t) | 8 | 8 (moved after `u`) |
| `data_offset` (size_t) | 8 | — (in union) |
| `data_size` (size_t) | 8 | — (in union) |
| `atom` (4 tag + 4 pad + 16 union) | 24 | — |
| `type` (pn_type_t) | — | 4 (offset 0) |
| `described` + `small` (2 bool) | — | 2 (offset 4–5) |
| `next` (pni_nid_t, uint16_t) | — | 2 (offset 6, fills gap) |
| `u` (8-byte union, holds `array_type`) | — | 8 (offset 8, aligned ✓) |
| `start` (size_t) | — | 8 (offset 16, aligned ✓, zero padding) |
| `prev`…`children` (4 × uint16_t) | — | 8 (offset 24) |
| old `described`+`data`+`small`+pad | 8 | — |
| old `type` (array element) | 4 | — (in union) |
| old 5× nid + pad | 16 | — |
| **Total** | **72** | **32** |

**40 bytes saved per node (56% reduction).** With a default cap of 1024 nodes
for performative fields this saves 40 KB of live heap. Once Sub-Task 3 of
`pni-node-restructure-plan.md` removes `start`, the struct shrinks to
**24 bytes** (67% reduction, 48 KB saved at the 1024-node cap).

---

## New types (in `data.h`)

```c
/*
 * Value payload for a pni_node_t.
 *
 * BINARY/STRING/SYMBOL/DECIMAL128/UUID nodes all store their byte data in
 * the intern buffer (data->buf); the offset and size within that buffer
 * live in as_bytes.  A pn_bytes_t / pn_decimal128_t / pn_uuid_t is
 * synthesised on read-out using pn_buffer_memory(data->buf).
 * DECIMAL128 and UUID always have as_bytes.size == 16.
 *
 * PN_ARRAY nodes store the element type in array_type.
 *
 * Compound/null types (LIST, MAP, DESCRIBED, NULL) have no payload;
 * only the type tag is meaningful.
 */
typedef union {
  /* scalars — largest is 8 bytes (double / uint64_t / int64_t) */
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
  /* interned data: BINARY, STRING, SYMBOL, DECIMAL128, UUID */
  struct {
    uint32_t      offset;         /* byte offset into data->buf */
    uint32_t      size;           /* byte count (always 16 for decimal128/uuid) */
  }               as_bytes;
  /* PN_ARRAY element type */
  pn_type_t       array_type;
} pni_node_payload_t;            /* 8 bytes */
```

`type` is the first field; `described`, `small`, and `next` fill the 4-byte
alignment gap before `u`; `start` sits immediately after `u` so it is
naturally 8-byte aligned with no padding; the remaining nid fields trail:

```c
typedef struct {
  pn_type_t           type;        /* value type tag          — offset  0    */
  bool                described;   /* PN_ARRAY: has descriptor child — off 4  */
  bool                small;       /* encoder scratch         — offset  5    */
  pni_nid_t           next;        /* fills alignment gap     — offset  6    */
  pni_node_payload_t  u;           /* value payload           — offset  8    */
  size_t              start;       /* encoder scratch         — offset 16    */
  pni_nid_t           prev;        /*                         — offset 24    */
  pni_nid_t           down;        /*                         — offset 26    */
  pni_nid_t           parent;      /*                         — offset 28    */
  pni_nid_t           children;    /*                         — offset 30    */
} pni_node_t;                      /* 32 bytes — no padding anywhere */
```

---

## Sub-Task — Define `pni_node_payload_t` and migrate `pni_node_t`

**Status:** `[ ] pending`

**Intent:** Replace `pn_atom_t atom`, `size_t data_offset`, `size_t data_size`,
`bool data`, and `pn_type_t type` (array element) in `pni_node_t`. The value
type tag becomes the first field `type`. `described`, `small`, and `next` fill
the 4-byte alignment gap before `u`. The array element type lives in
`u.array_type`. `start` moves after `u` and the nid fields.
`PN_BINARY`/`PN_STRING`/`PN_SYMBOL`/`PN_DECIMAL128`/`PN_UUID` all use
`u.as_bytes.{offset,size}`. No pointer is ever stored in a node; values are
synthesised from the intern buffer on read-out. This subsumes
`pni-node-restructure-plan.md` Sub-Task 2 (eliminate `pni_data_rebase`): with
no pointer in the node there is nothing to patch on realloc.

**Expected Outcomes:**

- `pni_node_t` has no `atom`, `data_offset`, `data_size`, or `data` fields.
- The old `pn_atom_t atom` → `pn_type_t type` (first field) + `pni_node_payload_t u`.
- The old separate `pn_type_t type` (array element) → `u.array_type`.
- `next` moves from its current position to offset 6 (filling the gap).
- `pni_data_add` (node initialisation) no longer zeroes `data_offset`,
  `data_size`, or sets `data = false`.
- `pni_data_intern_node` receives `pn_bytes_t bytes` as a parameter, writes
  `node->u.as_bytes.offset` and `node->u.as_bytes.size`, and does **not**
  call `pni_data_rebase`.
- `pni_data_bytes` and `pni_data_rebase` are deleted entirely.
- All `pn_data_put_binary/string/symbol` set `node->type` and pass `bytes`
  directly to `pni_data_intern_node`.
- `pn_data_put_decimal128` and `pn_data_put_uuid` also intern into `data->buf`
  via `pni_data_intern_node`, passing the 16 raw bytes as a `pn_bytes_t`.
- `pn_data_get_binary/string/symbol/bytes` materialise:
  `(pn_bytes_t){ node->u.as_bytes.size, pn_buffer_memory(data->buf).start + node->u.as_bytes.offset }`.
- `pn_data_get_decimal128` materialises by copying 16 bytes from
  `pn_buffer_memory(data->buf).start + node->u.as_bytes.offset` into a
  `pn_decimal128_t` and returning it by value.
- `pn_data_get_uuid` does the same for `pn_uuid_t`.
- `pn_data_get_atom` synthesises a `pn_atom_t` on the stack, filling
  `as_bytes` for string types, `as_decimal128`/`as_uuid` for those types,
  all from the intern buffer.
- `pni_inspect_enter` builds a stack-local `pn_atom_t tmp` from `node->type`
  and `node->u`, materialising bytes/decimal128/uuid from the intern buffer,
  then passes `&tmp` to `pni_inspect_atom` (unchanged signature).
- `pn_node2code` in `encoder.c` reads `node->u.as_bytes.size` for
  STRING/BINARY/SYMBOL opcode selection.
- `pni_encoder_enter` accesses `node->u.*` directly; for
  `PNE_VBIN*/PNE_STR*/PNE_SYM*` materialises `pn_bytes_t` from
  `node->u.as_bytes.{offset,size}`; for `PNE_DECIMAL128`/`PNE_UUID` reads
  16 bytes from `pn_buffer_memory(data->buf).start + node->u.as_bytes.offset`.
- `pn_data_put_atom` deconstructs the incoming `pn_atom_t`: for
  BINARY/STRING/SYMBOL/DECIMAL128/UUID calls the corresponding put-function;
  for all other types sets `node->type = atom.type` and copies the scalar from
  `atom.u` into `node->u`.
- Array element type: old `node->type` → `node->u.array_type`.
- `node->data` references everywhere are removed (concept gone).
- All existing tests pass.
- `pni-node-restructure-plan.md` Sub-Task 2 is obsolete (subsumed).

**Todo:**

1. **`data.h`**: define `pni_node_payload_t`; rewrite `pni_node_t` as shown
   above (`type` first, `described`/`small`/`next` in the gap, `array_type`
   in the union, `start` last).

2. **`codec.c` — `pni_data_add`**: remove `node->data = false`,
   `node->data_offset = 0`, `node->data_size = 0` from node initialisation.

3. **`codec.c` — `pni_data_intern_node`**: change signature to
   `pni_data_intern_node(pn_data_t *data, pni_node_t *node, pn_bytes_t bytes)`;
   remove the `pni_data_bytes` call; write `node->u.as_bytes.offset` and
   `node->u.as_bytes.size`; remove the capacity-change detection block and
   the `pni_data_rebase` call.

4. **`codec.c` — delete `pni_data_bytes` and `pni_data_rebase`**.

5. **`codec.c` — `pn_data_put_binary/string/symbol`**: remove the
   `node->atom.u.as_bytes = bytes` line; set `node->type`; call
   `pni_data_intern_node(data, node, bytes)` directly.

   **`codec.c` — `pn_data_put_decimal128/uuid`**: replace the `memmove` into
   the node with `node->type = PN_DECIMAL128/PN_UUID` and
   `pni_data_intern_node(data, node, (pn_bytes_t){16, (char*)d.bytes})`.

6. **`codec.c` — all other `pn_data_put_*`**: rename
   `node->atom.type` → `node->type`, `node->atom.u.as_*` → `node->u.as_*`.
   For `pn_data_put_null`: `node->type = PN_NULL` (no payload needed).
   For `pn_data_put_array`: set `node->u.array_type = type` instead of
   `node->type = type`.

7. **`codec.c` — `pn_data_put_atom`**: switch on `atom.type`; for
   BINARY/STRING/SYMBOL/DECIMAL128/UUID call the corresponding put-function;
   otherwise set `node->type = atom.type` and copy the scalar from `atom.u`
   into `node->u`.

8. **`codec.c` — `pn_data_get_binary/string/symbol/bytes`**: materialise
   `(pn_bytes_t){ node->u.as_bytes.size, pn_buffer_memory(data->buf).start + node->u.as_bytes.offset }`.

9. **`codec.c` — `pn_data_get_decimal128/uuid`**: materialise by copying 16
   bytes from `pn_buffer_memory(data->buf).start + node->u.as_bytes.offset`
   into the return value.

10. **`codec.c` — `pn_data_get_atom`**: for `PN_BINARY`/`PN_STRING`/`PN_SYMBOL`
    build `pn_atom_t tmp = {node->type}` with `tmp.u.as_bytes` materialised;
    for `PN_DECIMAL128`/`PN_UUID` materialise into `tmp.u.as_decimal128` /
    `tmp.u.as_uuid`; for all others copy `node->type` and `node->u` directly.

11. **`codec.c` — `pni_inspect_enter`**: synthesise `pn_atom_t tmp` from
    `node->type` and `node->u` (materialising bytes/decimal128/uuid from the
    intern buffer); pass `&tmp` to `pni_inspect_atom`.

12. **`codec.c` — all remaining `node->atom.type` / old `node->type` /
    `node->data` references**: `node->atom.type` → `node->type`; old
    `node->type` (array element) → `node->u.array_type`; remove all
    `node->data` checks.

13. **`encoder.c` — `pn_node2code`**: replace `node->atom.u.as_bytes.size`
    with `node->u.as_bytes.size`; `node->atom.type` → `node->type`.

14. **`encoder.c` — `pni_encoder_enter`**: replace `pn_atom_t *atom = &node->atom`
    with direct `node->u.*` access; for `PNE_VBIN*/PNE_STR*/PNE_SYM*`
    materialise `pn_bytes_t` from `node->u.as_bytes.{offset,size}` and pass
    to `pn_encoder_writev8/32`; for `PNE_DECIMAL128`/`PNE_UUID` pass
    `pn_buffer_memory(data->buf).start + node->u.as_bytes.offset` directly to
    `pn_encoder_writef128`.

15. **`encoder.c` — `pni_encoder_exit` and helpers**: `node->atom.type` →
    `node->type`; old `node->type` (array element) → `node->u.array_type`.

**Relevant files/symbols:**
- `c/src/core/data.h` — `pni_node_t`, `pni_inspect_atom` declaration
- `c/src/core/codec.c` — `pni_data_add`, `pni_data_intern_node`,
  `pni_data_bytes`, `pni_data_rebase`, all `pn_data_put_*` / `pn_data_get_*`,
  `pn_data_get_atom`, `pn_data_put_atom`, `pni_inspect_enter`,
  `pni_inspect_exit`, `pni_next_nonnull`, `pni_node_fields`,
  `pn_data_type`, `pni_data_parent_type`
- `c/src/core/encoder.c` — `pn_node2code`, `pni_encoder_enter`,
  `pni_encoder_exit`, `pn_is_in_array`, `pn_is_in_described_list`

---

## Notes

- `pn_buffer_memory(data->buf)` is called in several places to materialise the
  bytes pointer; it is cheap (returns a struct by value from a simple field
  access). It must only be called when `data->buf != NULL`, which is always true
  for any node that has been put with a buffer-backed type (binary, string,
  symbol, decimal128, uuid) since `pni_data_intern_node` creates the buffer
  on first use.
- The `as_bytes.offset` field is `uint32_t` (not `size_t`). This is valid
  because `data->buf` is bounded: the intern buffer cannot exceed
  `max_buf_size` (which defaults to the input byte count, itself bounded by the
  uint16_t node-count limit and the transport's delivery-size limit).
- `decimal128` and `uuid` are decoded by [`decoder.c`](c/src/core/decoder.c)
  via `pn_data_put_decimal128`/`pn_data_put_uuid` and re-encoded via
  `pn_encoder_writef128`. Interning them into `data->buf` is correct: the 16
  bytes are opaque and the intern buffer is not null-terminated for binary data
  (the trailing `\0` that `pni_data_intern` appends is harmless, as the size
  stored in `as_bytes.size` is always 16, not 17).
- `pni_inspect_atom` keeps its `pn_atom_t *` signature; synthesising the atom
  on the stack is a trivial compound-literal assignment.
- This plan subsumes `pni-node-restructure-plan.md` Sub-Task 2 entirely.
  Sub-Tasks 3 and 4 of that plan (`start`/`small` removal and small-encoding)
  are unaffected and follow naturally after this one.
- The `small` flag is renamed but otherwise unchanged; it is still the target
  of Sub-Task 3 of `pni-node-restructure-plan.md`.
