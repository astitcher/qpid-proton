# `pn_atom_t` / `pn_amqp_value_t` C-Standard Compatibility Plan

## Top-Level Overview

Three families of struct pointer casts in the new `amqp_value` code rely on
layout compatibility between distinct struct types. Each family currently works
in practice but is either undefined behaviour under the C standard, or relies on
a guarantee that is not actively enforced. This plan audits each family and
applies the minimal fix that makes the code strictly conforming, without changing
any public API.

The three families are:

1. **`pn_amqp_value_t*` ↔ `pn_atom_t*`** — direct pointer casts used to reuse
   `emit_atom` / `consume_atom` for the scalar subset of `pn_amqp_value_t`.
2. **`pn_amqp_compound_t*` ↔ `pn_amqp_value_t*`** — `pn_amqp_compound_t` (a
   heap-allocated flexible-array struct) is returned and accepted as
   `pn_amqp_value_t*` by the public API, with the caller discriminating on the
   initial `compound_type` field.
3. **`pn_amqp_map_t*` / `pn_amqp_list_t*` / `pn_amqp_array_t*` ↔
   `pn_amqp_compound_t*`** — opaque public typedefs that are all
   `pn_amqp_compound_t` under the hood.

---

## Background — Relevant C Standard Rules

### Common Initial Sequence (CIS) — C11 §6.5.2.3p6

Two structs share a **common initial sequence** if their leading members have
compatible types in the same order. The standard guarantees that reading a common
initial member through a pointer to *either* struct type is defined behaviour —
**but only when both struct types are members of the same union** and the access
is made through that union object. Direct pointer casts between unrelated struct
types (not members of a union together) are **not** covered and constitute
undefined behaviour under strict aliasing rules (§6.5p7).

### `memcpy` — always safe

Copying between any two objects via `memcpy` is always defined provided the
sizes match. The test helper `amqp(pn_atom_t)` in `amqp_value_test.cpp` already
uses this approach.

### Flexible-array-member structs

A struct with a flexible array member (`uint8_t raw[]`) cannot be placed in a
union at all (C11 §6.7.2.1p3). The `pn_amqp_compound_t` family therefore
**cannot** use the CIS/union approach; it must rely on an explicit design
contract instead.

---

## Sub-Task 1 — Fix `pn_amqp_value_t` ↔ `pn_atom_t` pointer casts

**Status:** `[ ] pending`

**Intent:** Three sites cast a `pn_amqp_value_t*` to `pn_atom_t*` (or vice
versa) and pass it to `emit_atom` / `consume_atom`. These functions only read /
write the initial `type`/`value_type` discriminant and the `u` union. Because
neither struct is a member of a union that contains both types, the casts are UB
under strict aliasing rules.

The correct fix is to place both struct types as members of a **wrapper union**
in a private header so that the CIS rule applies. Field names do not need to
match — the CIS rule requires only that the *types* of leading members are
compatible (C11 §6.5.2.3p6). `pn_type_t type` and `pn_type_t value_type` are
the same type, so the CIS requirement is already satisfied by content; the only
missing ingredient is the shared union. No rename is needed.

**Expected Outcomes:**
- A new internal header `c/src/core/amqp_value_compat.h` defines:
  ```c
  union pni_atom_value_u {
    pn_atom_t       as_atom;
    pn_amqp_value_t as_value;
  };
  ```
  plus a `_Static_assert` that `sizeof(pn_atom_t) == sizeof(pn_amqp_value_t)`.
- The four cast sites (`amqp_value.c:136`, `amqp_value.c:181`,
  `amqp_value.c:465`, `message.c:1159`) are replaced with round-trips through
  the union:
  ```c
  union pni_atom_value_u tmp = { .as_value = *v };
  emit_atom(emitter, compound, &tmp.as_atom);
  ```
- `pn_amqp_value_t::value_type` is **not renamed**; the public header is
  unchanged.
- `amqp_value_test.cpp`'s `amqp()` helper — which uses `memcpy` — continues to
  work; `memcpy` is always safe and the union approach is strictly conforming
  regardless.
- All existing tests pass.

**Todo:**
1. Create `c/src/core/amqp_value_compat.h` with the union definition and the
   `_Static_assert`.
2. In `amqp_value.c` and `message.c`, replace the four `(pn_atom_t*) v`-style
   casts with union-mediated access using `pni_atom_value_u`.
3. Add `#include "amqp_value_compat.h"` to `amqp_value.c` and `message.c`.

**Relevant files/symbols:**
- `c/include/proton/amqp_value.h` — `pn_amqp_value_t` (unchanged)
- `c/src/core/amqp_value.c` — `emit_amqp_value` (line 136),
  `consume_amqp_value` (line 181), `pn_message_properties_next` (line 465)
- `c/src/core/message.c` — `pn_message_set_body_value` (line 1159)
- `c/tests/amqp_value_test.cpp` — `amqp()` helper (uses `memcpy`; verify still
  compiles and that `sizeof` assertion holds)

---

## Sub-Task 2 — Document and enforce `pn_amqp_compound_t` ↔ `pn_amqp_value_t` contract

**Status:** `[ ] pending`

**Intent:** `pn_amqp_compound_t` (a flexible-array-member struct) is returned
as `pn_amqp_value_t*` from `pn_message_get_body_value` and cast back inside
`pn_amqp_value_bytes` and `pn_amqp_value_free`. This is a deliberate
type-punning design: the first field `compound_type` (`pn_amqp_compund_type_t`,
values 128–130) falls in the range that cannot overlap with any `pn_type_t`
value (1–25), so runtime discrimination via the common initial `int`-sized field
is correct.

However, C11 §6.5.2.3p6 cannot protect this family through the union trick
because flexible-array-member structs cannot be union members. The guarantee
here is architectural (first-member same-type, value ranges non-overlapping) but
is not currently checked. This sub-task adds explicit compile-time and runtime
guards.

**Expected Outcomes:**
- A `_Static_assert` in `amqp_value.c` (near the `pn_amqp_compound_t`
  definition) asserts:
  - `offsetof(pn_amqp_compound_t, compound_type) == offsetof(pn_amqp_value_t, type)`
  - `sizeof(((pn_amqp_compound_t*)0)->compound_type) == sizeof(((pn_amqp_value_t*)0)->type)`
- A comment block above `pn_amqp_compound_t` explains the design contract:
  - The cast `pn_amqp_compound_t* → pn_amqp_value_t*` is valid by
    implementation-defined layout rules (GCC/Clang guarantee first-member
    placement even for flexible-array structs); the discriminant value ranges
    (128–130 vs 1–25) guarantee correct runtime dispatch.
  - The reverse cast back to `pn_amqp_compound_t*` is only performed after
    confirming `value_type >= PN_COMPOUND_ARRAY` (128), which is enforced by
    every call site.
- A `_Static_assert` on `PN_COMPOUND_ARRAY > PN_MAP` (128 > 25) guards against
  future enum collision.
- All existing tests pass.

**Todo:**
1. In `c/src/core/amqp_value.c`, after the `pn_amqp_compound_t` struct
   definition (line ~45), add:
   ```c
   _Static_assert(
     offsetof(pn_amqp_compound_t, compound_type) == offsetof(pn_amqp_value_t, type),
     "pn_amqp_compound_t and pn_amqp_value_t must share first-member offset");
   _Static_assert(
     sizeof(((pn_amqp_compound_t*)0)->compound_type) == sizeof(((pn_amqp_value_t*)0)->type),
     "pn_amqp_compound_t and pn_amqp_value_t discriminants must be the same size");
   _Static_assert(PN_COMPOUND_ARRAY > PN_MAP,
     "pn_amqp_compund_type_t values must not overlap pn_type_t values");
   ```
2. Add a design-contract comment block above `pn_amqp_compound_t` explaining the
   cast semantics (see Expected Outcomes above).
3. Check every reverse cast site (`pni_amqp_compound_bytes`, `pn_amqp_value_bytes`,
   `pn_amqp_value_free`) to confirm each is guarded by a `value->type >=
   PN_COMPOUND_ARRAY` check; add an assertion or early-return where missing.

**Relevant files/symbols:**
- `c/src/core/amqp_value.c` — `pn_amqp_compound_t` definition (line 39),
  `pn_amqp_value_bytes` (line 233), `pn_amqp_value_free` (line 255),
  `pn_message_get_body_value` (`message.c:1135`)
- `c/include/proton/amqp_value.h` — `pn_amqp_compund_type_t` enum

---

## Sub-Task 3 — Fix opaque typedef casts (`pn_amqp_map_t*` etc.)

**Status:** `[ ] pending`

**Intent:** `pn_amqp_map_t`, `pn_amqp_list_t`, and `pn_amqp_array_t` are
declared as opaque `typedef struct … T` in `amqp_value.h` and then cast to/from
`pn_amqp_compound_t*` throughout `amqp_value.c`, and to `pn_amqp_value_t*` in
the example programs. These casts are essentially the same family as Sub-Task 2
but are one step further removed: the examples cast `pn_amqp_map_t*` directly to
`pn_amqp_value_t*`, bypassing `pn_amqp_compound_t*`.

Because `pn_amqp_map_t` is an incomplete type (opaque struct), the cast is
implementation-defined but works on every target. The clean fix is to give the
opaque types explicit definitions as aliases of `pn_amqp_compound_t` in the
private header, so the chain `pn_amqp_map_t*` → `pn_amqp_compound_t*` →
`pn_amqp_value_t*` is explicit and each hop is justified.

**Expected Outcomes:**
- In `c/src/core/amqp_value_private.h` (or a new `amqp_compound_types.h`),
  define:
  ```c
  typedef pn_amqp_compound_t pn_amqp_map_t;
  typedef pn_amqp_compound_t pn_amqp_list_t;
  typedef pn_amqp_compound_t pn_amqp_array_t;
  ```
  so that internally the types are identical and no pointer cast is needed
  between them and `pn_amqp_compound_t`.
- The public `amqp_value.h` keeps the opaque forward declarations unchanged
  (`typedef struct pn_amqp_map_t pn_amqp_map_t;`) so the public API is stable.
- All internal cast sites inside `amqp_value.c` that cast between
  `pn_amqp_map_t*` / `pn_amqp_list_t*` / `pn_amqp_array_t*` and
  `pn_amqp_compound_t*` become direct assignments (no cast needed once the
  typedef aliases are in scope).
- Example programs (`send.c`, `send-ssl.c`, `direct.c`) that cast
  `pn_amqp_map_t*` to `pn_amqp_value_t*` are updated to go through the
  two-step: first cast to `pn_amqp_compound_t*` (trivial, same type), then use
  the compound → value path already validated in Sub-Task 2.
- All existing tests pass.

**Todo:**
1. In `c/src/core/amqp_value_private.h`, add the three typedef aliases after
   `#include <proton/amqp_value.h>`.
2. Remove all `(pn_amqp_compound_t*)` casts from `pn_amqp_map_*`,
   `pn_amqp_list_*`, `pn_amqp_array_*` functions in `amqp_value.c` — they
   become unnecessary.
3. In example files (`c/examples/send.c`, `send-ssl.c`, `direct.c`), replace
   `(pn_amqp_value_t*)props` with an explicit intermediate:
   ```c
   pn_amqp_value_t *body = (pn_amqp_value_t*)(pn_amqp_compound_t*)props;
   pn_message_set_body_value(app->message, body);
   ```
   (or a helper inline if preferred).

**Relevant files/symbols:**
- `c/src/core/amqp_value_private.h`
- `c/src/core/amqp_value.c` — all `(pn_amqp_compound_t*)map/list/array` casts
- `c/include/proton/amqp_value.h` — opaque typedefs (unchanged)
- `c/examples/send.c`, `c/examples/send-ssl.c`, `c/examples/direct.c`

---

## Notes

- Sub-Tasks 1, 2, and 3 are independent and can be done in any order.
- Sub-Task 1 makes **no public header changes** — the CIS rule requires only
  compatible *types* for leading members, not matching field names.
  `pn_type_t value_type` and `pn_type_t type` are the same type; the only fix
  needed is a shared union in a private header.
- No changes to `pn_atom_t` in `codec.h`; no changes to `pni_node_t` in
  `data.h`.
- The `pni-node-restructure-plan.md` sub-tasks are entirely unaffected: they
  operate on `pni_node_t::atom` (a `pn_atom_t`) inside `codec.c` and
  `encoder.c`, which do not touch `pn_amqp_value_t` at all.
- After this plan, a follow-on `_Static_assert(sizeof(pn_atom_t) ==
  sizeof(pn_amqp_value_t))` in the compat header guards against any future
  divergence.
