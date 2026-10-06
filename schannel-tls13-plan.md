# Schannel TLS 1.3 support — implementation plan

Status: planning. Supersedes the WIP diff currently in the worktree
(`c/src/ssl/schannel.cpp`, unstaged, 34+/4-).

Target: `c/src/ssl/schannel.cpp` only. No public API change required, though
`pn_ssl_get_protocol_name()` gains a value that `c/include/proton/ssl.h:246`
already documents.

Assumes an implementer with an interactive Windows machine: a recent MSVC +
Windows SDK, and ideally **two** test hosts — one Windows 11 / Server 2022+
(TLS 1.3 capable) and one Windows 10 22H2 or Server 2019 (TLS 1.2 only, still
`SCH_CREDENTIALS` capable). A Windows 8.1 / Server 2012 R2 VM is desirable for
phase 1 but can be substituted with a forced-fallback build flag.

---

## 1. Goal and constraint

Negotiate TLS 1.3 on Windows where the OS supports it, while keeping the
declared Windows 7 floor (`_WIN32_WINNT=0x0601`, `CMakeLists.txt:155`, set
deliberately by 092232d2a) working from a single binary.

Schannel only offers TLS 1.3 to callers that pass the newer
[`SCH_CREDENTIALS`](https://learn.microsoft.com/en-us/windows/win32/api/schannel/ns-schannel-sch_credentials)
struct to `AcquireCredentialsHandle`; the
[`SCHANNEL_CRED`](https://learn.microsoft.com/en-us/windows/win32/api/schannel/ns-schannel-schannel_cred)
struct we use today is explicitly deprecated ("You should use SCH_CREDENTIALS
instead") and caps out at TLS 1.2.

Two version floors matter and they are different:

| | Minimum |
|---|---|
| `SCH_CREDENTIALS` accepted by `AcquireCredentialsHandle` | Windows 10 1809 / Server 2019 |
| TLS 1.3 actually negotiable | **Windows 11 / Server 2022** |

Per [Protocols in TLS/SSL (Schannel SSP)](https://learn.microsoft.com/en-us/windows/win32/secauthn/protocols-in-tls-ssl--schannel-ssp-):
"TLS 1.3 is supported starting in Windows 11 and Windows Server 2022. Enabling
TLS 1.3 on earlier versions of Windows is not a safe system configuration." So
on Win10 1809–22H2 this work buys only the non-deprecated API; the protocol
upgrade lands on Win11/2022+.

Useful: GitHub Actions `windows-latest` is Server 2022 or later, so CI will
exercise the TLS 1.3 path once phase 1 lands. That is both an opportunity and a
hazard — see §7.

---

## 2. What is wrong with the WIP diff

Five defects, in the order they would bite. Items A and B are in the new code;
C, D and E are pre-existing code that TLS 1.3 will newly expose.

### A. The new branch never compiles in

`schannel.cpp:239` guards on `#ifdef SCH_CREDENTIALS_VERSION`, commented
"present in modern Windows 10 SDKs". That is the wrong model. In both the
Windows SDK and mingw-w64, `SCH_CREDENTIALS`, `TLS_PARAMETERS`,
`CRYPTO_SETTINGS` and `SCH_CREDENTIALS_VERSION` sit inside a single
`#ifdef SCHANNEL_USE_BLACKLISTS` block — not an `NTDDI_VERSION` /
`_WIN32_WINNT` gate. From the SCH_CREDENTIALS Remarks:

> To use the SCH_CREDENTIALS structure, define `SCHANNEL_USE_BLACKLISTS` along
> with `UNICODE_STRING` and `PUNICODE_STRING`. Alternatively, include Ntdef.h,
> SubAuth.h or Winternl.h.

`SCHANNEL_USE_BLACKLISTS` appears nowhere in the repo and the include block at
`schannel.cpp:49-55` pulls in none of those headers. The macro is therefore
always undefined, the `#else` (old `SCHANNEL_CRED`) arm always compiles, and
the diff is a no-op on every SDK.

### B. Compile-time selection breaks the Win7 floor

Even once A is fixed, `#ifdef` picks the struct at *build* time. A binary built
against a current SDK would hand `dwVersion == SCH_CREDENTIALS_VERSION` (5) to
a Win7/8.1/Server 2012 R2 Schannel that has never heard of it;
`AcquireCredentialsHandle` fails and all TLS dies on those platforms. The
choice has to be made at runtime.

### C. `SEC_I_RENEGOTIATE` is fatal — this kills every TLS 1.3 connection

`schannel.cpp:1071-1073` logs "unexpected TLS renegotiation" and falls through
to `ssl_failed()`. From
[DecryptMessage (Schannel)](https://learn.microsoft.com/en-us/windows/win32/secauthn/decryptmessage--schannel):

> **DecryptMessage (Schannel)** returns SEC_I_RENEGOTIATE when it receives a
> post-handshake TLS protocol message other than application data. Once
> DecryptMessage returns SEC_I_RENEGOTIATE, any further EncryptMessage() and
> DecryptMessage() calls fail with SEC_E_CONTEXT_EXPIRED. An application handles
> this situation by calling AcceptSecurityContext (server side) or
> InitializeSecurityContext (client side) and passing the same buffer as
> modified by DecryptMessage, ensuring that the `SecBuffer` type is set to
> `SECBUFFER_TOKEN`. Note that **DecryptMessage doesn't always return an
> `SECBUFFER_EXTRA` buffer** when SEC_I_RENEGOTIATE is returned.

In TLS 1.3 this is the normal path, not an edge case: Schannel reuses the
renegotiation code path to surface `NewSessionTicket` and `KeyUpdate`. Servers
routinely send tickets immediately after the handshake, so with TLS 1.3
enabled essentially every connection dies within the first few
`DecryptMessage` calls. This is the single most-reported bug when projects
switch to `SCH_CREDENTIALS`.

There is a secondary data-loss bug here: `schannel.cpp:1062-1063` calls
`rewind_sc_inbuf(ssl)` **before** the status switch, and the output buffers
have not yet been scanned for `SECBUFFER_EXTRA`, so `inbuf_extra` is still
null. `rewind_sc_inbuf` therefore sets `sc_in_count = 0` and discards exactly
the bytes the docs tell us to hand back as a `SECBUFFER_TOKEN`.

### D. The client rejects a final handshake token

`schannel.cpp:1217-1220`:

```c
if (send_buffs[0].cbBuffer != 0) {
  ssl_failed(transport, "unexpected final server token");
```

Correct for a TLS 1.2 Schannel client — the client Finished went out on the
previous `SEC_I_CONTINUE_NEEDED`. In TLS 1.3 the client's final flight
(Finished, plus client Certificate/CertificateVerify under mutual auth) is
produced by the *same* `InitializeSecurityContext` call that returns
`SEC_E_OK`. Dropping it leaves the server waiting and the connection times out
(or the peer sends an alert and we see `SEC_E_ILLEGAL_MESSAGE` 0x80090326).

Note the server path at `schannel.cpp:1403-1404` already does the right thing;
the client path is the asymmetric one. The MS
[AcceptSecurityContext (Schannel)](https://learn.microsoft.com/en-us/windows/win32/secauthn/acceptsecuritycontext--schannel)
page states outright that an output buffer may be generated even on
`SEC_E_OK` and must be sent; the same holds for the client.

### E. `pn_ssl_get_protocol_name()` doesn't know TLS 1.3

`schannel.cpp:815-825` tests `0xC0` / `0x300` / `0xC00` then falls to an error
branch that logs `"unexpected protocol"` and returns `false`.
`SP_PROT_TLS1_3_SERVER` is `0x1000` and `SP_PROT_TLS1_3_CLIENT` is `0x2000`, so
TLS 1.3 lands in the error arm.

This is a live test failure, not cosmetics:
`c/tests/proactor_test.cpp:558-559` does
`CHECK(pn_ssl_get_protocol_name(...)); CHECK_THAT(protocol, Contains("TLS"));`
and `c-proactor-test` runs on Windows.

`tls_version_check()` at `schannel.cpp:995` is fine — `0x3000 >
SP_PROT_TLS1_SERVER` (0x40) — no change needed there.

### Not defects

For the record, so these don't get re-litigated:

- The `SCH_CREDENTIALS` field mapping in the WIP diff is **correct**.
  `dwVersion`, `paCred`/`cCreds`, `hRootStore`, `dwFlags` carry over with
  identical semantics, and gating `hRootStore` on server mode matches the
  doc's "Valid for server applications only". Nothing is lost relative to
  `SCHANNEL_CRED`, because the old code never set `grbitEnabledProtocols`, the
  cipher-strength fields, or `palgSupportedAlgs`.
- The zeroed `TLS_PARAMETERS` with `cTlsParameters = 1` is legal and
  equivalent to the system default. (Docs: "It is an error to include more
  than one TLS_PARAMETERS structure with cAlpnIds == 0 and rgstrAlpnIds ==
  NULL" — one is fine.) It is also a stack local, which is safe because
  `AcquireCredentialsHandle` consumes it synchronously.

---

## 3. Phase 1 — switch to `SCH_CREDENTIALS` with a runtime fallback

Fixes A, B and E. Self-contained; leaves TLS 1.3 *reachable* but not yet
*working*, so **do not merge phase 1 without phase 2** (see §7).

### 3.1 Include preamble

`schannel.cpp:49-55`, before `<windows.h>`:

```c
// SCH_CREDENTIALS / TLS_PARAMETERS are gated on this macro in schannel.h,
// and need UNICODE_STRING, which plain windows.h does not provide.
#define SCHANNEL_USE_BLACKLISTS
#include <windows.h>
#include <winternl.h>
#define SECURITY_WIN32
#include <security.h>
#include <Schnlsp.h>
#include <WinInet.h>
#undef SECURITY_WIN32
```

`subauth.h` is a lighter alternative to `winternl.h` if the latter conflicts
with anything. msquic's `src/platform/tls_schannel.c` is the reference for this
preamble; for environments with neither header it hand-declares
`UNICODE_STRING` directly.

**Keep** `#ifdef SCH_CREDENTIALS_VERSION` as a secondary guard — old mingw-w64
headers predate the block entirely and the repo still has a MinGW path
(`c/CMakeLists.txt:186`). The macro just stops being the *primary* gate.

Verify the macro actually took effect before going further — a `#else
#error`, or build once with `/showIncludes` and confirm `winternl.h` is pulled
in ahead of `schannel.h`. The whole point of this phase is that the previous
attempt silently compiled the wrong arm.

### 3.2 Runtime selection in `win_credential_cred_handle()`

`schannel.cpp:225-292`. Restructure as: try `SCH_CREDENTIALS`; on any failure,
retry with `SCHANNEL_CRED`; remember the outcome so the probe is paid once, not
per client connection.

```c
// -1 untested, 1 SCH_CREDENTIALS works, 0 fall back to SCHANNEL_CRED.
// Written at most once per process; benign if two threads race to the same value.
static LONG sch_credentials_usable = -1;
```

Shape:

```c
static SECURITY_STATUS acquire_modern(win_credential_t *cred, ULONG direction,
                                      CredHandle *out, TimeStamp *expiry);  // SCH_CREDENTIALS
static SECURITY_STATUS acquire_legacy(win_credential_t *cred, ULONG direction,
                                      CredHandle *out, TimeStamp *expiry);  // SCHANNEL_CRED
```

`win_credential_cred_handle()` then:

1. server cache check (unchanged, `:227-230`);
2. if `sch_credentials_usable != 0`, call `acquire_modern()`;
3. if that fails and `sch_credentials_usable == -1`, log the status at DEBUG,
   set `sch_credentials_usable = 0`, and call `acquire_legacy()`;
4. on success from `acquire_modern()`, set `sch_credentials_usable = 1`;
5. server cache store (unchanged, `:288-289`).

Fall back on *any* failure rather than matching a specific `SECURITY_STATUS`.
The error old Schannel returns for `dwVersion == 5` is not documented, and
guessing it wrong turns a graceful degrade into a hard failure. The cost of
being over-broad is one extra `AcquireCredentialsHandle` call on a genuinely
broken credential, which then fails again on the legacy path and reports the
legacy error — fine.

A build-time escape hatch is worth adding for testing:
`-DPN_SCHANNEL_FORCE_LEGACY_CRED` forcing `sch_credentials_usable = 0` at
start. Without it you cannot exercise the fallback arm unless you actually have
a Server 2012 R2 VM.

While here, scope the client-only flags correctly — `SCH_CRED_NO_DEFAULT_CREDS`
and `SCH_CRED_MANUAL_CRED_VALIDATION` are documented "Client only" but
`:246`/`:269` set them unconditionally. Schannel ignores them server-side so
this is cleanup, not a fix; do it in both helpers or neither.

Keep `cTlsParameters = 1` with the zeroed `TLS_PARAMETERS`, and say why in the
comment: `grbitDisabledProtocols` is the only hook for ever implementing
`pn_ssl_domain_set_protocols()`, which currently just returns `PN_ERR` at
`schannel.cpp:673-676` while the OpenSSL backend supports it.

### 3.3 `pn_ssl_get_protocol_name()`

`schannel.cpp:815-825`, add before the error arm:

```c
else if ((info.dwProtocol & 0x3000))     // SP_PROT_TLS1_3_{SERVER,CLIENT}
  snprintf(buffer, size, "%s", "TLSv1.3");
```

Match the existing style — the surrounding branches use bare hex with a
comment rather than the `SP_PROT_*` names, because those weren't defined in old
toolchains. Follow that.

### 3.4 Phase 1 acceptance

- Builds clean on MSVC and, if the MinGW path is still cared about, MinGW.
- Win11/2022 host: `ctest` run shows connections now negotiating TLS 1.3 —
  **and** the `c-proactor-test` SSL case failing in `ssl_decrypt` shortly after
  handshake. That failure is expected and is the proof phase 1 worked; it is
  defect C and phase 2 fixes it.
- Win10 22H2 / Server 2019 host: full suite green, `SCH_CREDENTIALS` in use
  (confirm by breakpoint or trace), protocol reported as TLSv1.2.
- `-DPN_SCHANNEL_FORCE_LEGACY_CRED` build: full suite green on any host.
- Win8.1/2012R2 if available: full suite green via automatic fallback, with the
  fallback logged once.

---

## 4. Phase 2 — post-handshake continuation (`SEC_I_RENEGOTIATE`)

Fixes C and D. This is the real work.

### 4.1 Why the existing state machine mostly fits

The ordered enum is
`CREATED, CLIENT_HELLO, NEGOTIATING, RUNNING, SHUTTING_DOWN, SSL_CLOSED`
(`schannel.cpp:315-316`), and three existing behaviours line up with what a
post-handshake continuation needs:

- `process_input_ssl:1724` routes input to `ssl_handshake()` when
  `state == NEGOTIATING`;
- `process_output_ssl:1829` only encrypts app data when `state == RUNNING`,
  which is exactly the suspension we need (`EncryptMessage` would fail with
  `SEC_E_CONTEXT_EXPIRED`);
- handshake tokens already leave via the raw `network_out_pending` path, not
  via `EncryptMessage`.

So **re-entering `NEGOTIATING` from `RUNNING`** is the natural move, and the
ordering-sensitive comparisons (`state <= SHUTTING_DOWN` at `:1723`,
`state >= SHUTTING_DOWN` at `:1782`) stay correct because
`NEGOTIATING < RUNNING`. Prefer this to adding a new enum value, which would
mean auditing every ordered comparison.

Three things do **not** fit and must be handled explicitly — see 4.3.

### 4.2 `ssl_decrypt()` changes

`schannel.cpp:1052-1078`. Reorder so the buffer scan happens before the rewind:

1. Keep the `SEC_E_INCOMPLETE_MESSAGE` early return as-is (`:1054-1058`).
2. For `SEC_I_RENEGOTIATE`, run the `SECBUFFER_EXTRA` half of the scan loop
   currently at `:1083-1098` *first*, so `inbuf_extra` / `extra_count` are
   populated. Only pick up `SECBUFFER_EXTRA`; ignore `SECBUFFER_DATA` on this
   path. Then call `rewind_sc_inbuf()`, which will now preserve the leftover
   bytes by moving them to the front of `sc_inbuf` instead of dropping them.
3. Set `ssl->state = NEGOTIATING` and return `false`.
4. If there was no `SECBUFFER_EXTRA` (the docs warn this happens), `sc_in_count`
   ends at 0 and we simply wait for the next network read — the `NEGOTIATING`
   branch at `:1724` will pick it up. Do **not** call `ssl_handshake()` with an
   empty token from here.
5. Leave `SEC_I_CONTEXT_EXPIRED` and the `default` error arm unchanged.

Downgrade the "unexpected TLS renegotiation" error log to DEBUG/TRACE with new
wording — under TLS 1.3 this fires on every session ticket and would spam logs
at ERROR.

Also delete the duplicated `ssl->decrypting = false;` at `:1060` and `:1080`
while in the area.

### 4.3 The three mismatches

**(a) Output buffer collision.** `sc_outbuf` is single-buffered and shared
between handshake tokens and encrypted app records. `server_handshake:1426`
asserts `network_out_pending == 0`, and `client_handshake:1198` overwrites
`network_outp` unconditionally. During a *initial* handshake that assert holds
trivially; during a post-handshake continuation there may be encrypted app data
still in flight.

Guard it: in `process_input_ssl` at `:1723-1725`, only call `ssl_handshake()`
when `ssl->network_out_pending == 0`. If output is pending, leave the input
buffered and break out of the loop; `process_output_ssl` will drain and the
next input pass will run the handshake. Confirm the outer
`do { } while (available || ...)` at `:1780` cannot spin when this guard
blocks — if it can, the loop condition needs the same guard.

**(b) Re-running completion work.** `client_handshake`/`server_handshake` on
`SEC_E_OK` re-run `tls_version_check()`, `verify_peer()`, and the
`SECPKG_ATTR_STREAM_SIZES` query. Re-querying stream sizes is correct and
wanted. Re-running `verify_peer()` on every session ticket is a full chain
build — wasteful and noisy. Add a `bool handshake_complete` to `pni_ssl_t`,
set it on first `SEC_E_OK`, and skip peer verification when already set. Keep
the stream-sizes re-query and the `max > sc_out_size` check unconditional.

**(c) Client `SEC_E_OK` + token (defect D).** In `client_handshake` replace the
`:1217-1220` hard failure with the server's pattern: introduce a local
`outbound_token` flag, set it when `send_buffs[0].cbBuffer != 0` on the
`SEC_E_OK` arm, and queue the token after the switch exactly as
`server_handshake:1424-1432` does. The surrounding code already has the shape;
this is mostly a copy of the server arm, which is a mild argument for factoring
the common tail out of both functions — optional, and only if it comes out
genuinely cleaner.

### 4.4 What not to do

Do not attempt to support true TLS 1.2 renegotiation. If
`SEC_I_RENEGOTIATE` arrives on a TLS 1.2 connection, keep failing the
connection — renegotiation is a known attack surface and nothing in proton
needs it. Gate the new continuation path on the negotiated protocol being
TLS 1.3 (query `SECPKG_ATTR_CONNECTION_INFO`, check `dwProtocol & 0x3000`), or
accept the slightly looser behaviour and document the choice. Gating is
preferred.

### 4.5 Phase 2 acceptance

- Win11/2022: full `ctest` green, connections negotiating TLS 1.3, protocol
  name reported as `TLSv1.3`.
- A long-lived connection survives at least one `NewSessionTicket` and one
  `KeyUpdate`. Tickets usually arrive within the first few records; `KeyUpdate`
  needs to be provoked (see §6).
- Mutual auth (`PN_SSL_VERIFY_PEER` with a client cert) works under TLS 1.3 —
  this is the case where the client's `SEC_E_OK` flight is largest and defect D
  bites hardest.
- Clean shutdown still works: `ApplyControlToken` / `SCHANNEL_SHUTDOWN` at
  `schannel.cpp:1496-1508` drives `ssl_handshake()` with an empty token, and
  `shutdown` is computed as `state == SHUTTING_DOWN`. Check that a shutdown
  racing with a pending continuation does the right thing.
- Forced-legacy and Win10 builds unchanged (TLS 1.2 path must not regress).

---

## 5. Phase 3 — optional polish

Lower value; land separately or not at all.

- **Cipher name.** `pn_ssl_get_cipher_name()` (`schannel.cpp:798`) formats
  `ALG_ID`s from `SECPKG_ATTR_CONNECTION_INFO`; TLS 1.3 AEAD suites don't map
  onto those cleanly, so output will be odd but not wrong-looking enough to
  fail anything.
  [`SECPKG_ATTR_CIPHER_INFO` / `SecPkgContext_CipherInfo`](https://learn.microsoft.com/en-us/windows/win32/api/schannel/ns-schannel-secpkgcontext_cipherinfo)
  gives `szCipherSuite` as a string and is available from Vista, so it needs no
  version guard. Check what the OpenSSL backend returns and match the format.
- **`pn_ssl_get_ssf()`** (`:775`, already marked "untested guess") reads
  `dwCipherStrength`; `SecPkgContext_CipherInfo.dwCipherLen` is the better
  source once the above is in.
- **`pn_ssl_domain_set_protocols()`** (`:673-676`, currently `PN_ERR`) could now
  be implemented via `TLS_PARAMETERS.grbitDisabledProtocols`, bringing Schannel
  in line with OpenSSL. Note `c/tests/ssl_test.cpp` is **not** built on Windows
  (`c/tests/CMakeLists.txt:21-26` only sets `platform_test_src` in the non-WIN32
  branch), so enabling this means also enabling that test file on Windows —
  which will surface whatever else in it has drifted. Treat as its own task.

---

## 6. Windows test environment

**Hosts.** Win11 or Server 2022+ is mandatory (nothing below it can negotiate
TLS 1.3). Win10 22H2 or Server 2019 is strongly wanted to prove the
`SCH_CREDENTIALS`-but-TLS-1.2 middle case. Win8.1/Server 2012 R2 is nice to
have for the real fallback; `-DPN_SCHANNEL_FORCE_LEGACY_CRED` substitutes.

**Build.** As CI: `-A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake`,
`RelWithDebInfo`. Build at least once as `Debug` too, for the asserts in
`server_handshake` and `process_input_ssl` — several of the hazards in §4.3
show up as assertion failures rather than wrong behaviour.

**Tests that matter.** `c-proactor-test` (contains the
`pn_ssl_get_protocol_name` check at `c/tests/proactor_test.cpp:558`),
`c-ssl-proactor-test`, and the Python suite's SSL tests. Run the whole `ctest`
set — the SSL layer is reached indirectly from several places.

**Observing the protocol.** Set `PN_TRACE_FRM`/proton logging to DEBUG for the
SSL subsystem; `client handshake successful %zu max record size` and the
protocol-name path tell you which version landed. Wireshark on loopback
confirms independently and is worth doing once, since Schannel will silently
negotiate 1.2 if anything in the credential setup is off — a green test run
does **not** by itself prove TLS 1.3 was used.

**Provoking the hard cases.**
- *NewSessionTicket*: automatic on most TLS 1.3 servers. Proton-to-proton on
  Windows should produce them; confirm in Wireshark.
- *KeyUpdate*: hardest to trigger. Options are an OpenSSL 3.x `s_server` peer
  with `-keyupdate`, or pushing enough data to hit a rekey threshold. If it
  can't be provoked, say so in the commit message rather than claiming
  coverage.
- *Interop*: test proton-on-Windows against a non-Schannel TLS 1.3 peer
  (OpenSSL `s_server`/`s_client`, or a Linux-built proton) in both directions.
  Schannel-to-Schannel can mask bugs that only appear against another stack.

---

## 7. Sequencing and risk

**Phase 1 alone is not shippable.** On its own it enables TLS 1.3 negotiation
while defect C is still live, which breaks every TLS 1.3 connection — a
straight regression for anyone on Win11/Server 2022, and it will turn CI red
because `windows-latest` is TLS 1.3 capable. Phases 1 and 2 must land together,
or phase 1 must ship with TLS 1.3 explicitly disabled via
`TLS_PARAMETERS.grbitDisabledProtocols = SP_PROT_TLS1_3_CLIENT |
SP_PROT_TLS1_3_SERVER`, to be lifted by phase 2. The second option is the safer
one if phase 2 turns out to take longer than expected, and it costs two lines.

Suggested commits:

1. `PROTON-XXXX: Use SCH_CREDENTIALS on Windows where available` — §3, with
   TLS 1.3 disabled in `grbitDisabledProtocols`.
2. `PROTON-XXXX: Handle Schannel post-handshake messages (TLS 1.3)` — §4, and
   remove the disable from commit 1 in the same commit.
3. Optional §5 items, separately.

Both functional commits need a Windows reviewer; nothing here is verifiable
from Linux, and the failure modes (wrong `#ifdef` arm, silent downgrade to
1.2) are all silent.

---

## 8. Open questions to settle empirically on Windows

These are assumptions in the plan that could not be checked from Linux and
should be confirmed rather than trusted:

1. Does Schannel's TLS 1.3 client actually return `SEC_E_OK` *with* an output
   token (defect D), or does it return `SEC_I_CONTINUE_NEEDED` first? The MS
   docs mandate sending any returned token either way, so the fix is right
   regardless, but it changes whether D is a blocker or a latent bug.
2. What `SECURITY_STATUS` does pre-1809 Schannel return for `dwVersion == 5`?
   Only affects log quality, since the fallback triggers on any failure.
3. Does `SECBUFFER_EXTRA` in practice accompany `SEC_I_RENEGOTIATE` for
   `NewSessionTicket`, or is the "doesn't always" case the common one? Drives
   how much of §4.2 step 4 gets exercised.
4. Can a post-handshake continuation realistically coincide with pending
   encrypted output (§4.3a), or is the guard belt-and-braces? Worth an assert
   plus a log rather than assuming either way.

---

## 9. References

- [SCH_CREDENTIALS](https://learn.microsoft.com/en-us/windows/win32/api/schannel/ns-schannel-sch_credentials)
- [TLS_PARAMETERS](https://learn.microsoft.com/en-us/windows/win32/api/schannel/ns-schannel-tls_parameters)
- [SCHANNEL_CRED (deprecated)](https://learn.microsoft.com/en-us/windows/win32/api/schannel/ns-schannel-schannel_cred)
- [Protocols in TLS/SSL (Schannel SSP)](https://learn.microsoft.com/en-us/windows/win32/secauthn/protocols-in-tls-ssl--schannel-ssp-)
- [DecryptMessage (Schannel)](https://learn.microsoft.com/en-us/windows/win32/secauthn/decryptmessage--schannel)
- [AcceptSecurityContext (Schannel)](https://learn.microsoft.com/en-us/windows/win32/secauthn/acceptsecuritycontext--schannel)
- [Creating an Schannel Security Context](https://learn.microsoft.com/en-us/windows/win32/secauthn/creating-an-schannel-security-context)
- [SecPkgContext_CipherInfo](https://learn.microsoft.com/en-us/windows/win32/api/schannel/ns-schannel-secpkgcontext_cipherinfo)
- [msquic `tls_schannel.c`](https://github.com/microsoft/msquic/blob/main/src/platform/tls_schannel.c) — include preamble and `SCH_CREDENTIALS` setup
- [tlsuv PR #387](https://github.com/openziti/tlsuv/pull/387) — same migration, including the post-handshake fix
