# Private translator source control CPU prototype

This separate prototype composes the frozen getter adapter and native history
calculator. No vendor source is patched or built, no translator DLL is executed,
and no Wine, Vulkan, game or GPU operation is part of these CPU tests. Previous
V1/V2 wrappers, models, adapters, auditors and certificates remain unchanged.

The new source callbacks live in `native/translator_private_diagnostic`. The
source wrapper calls them around its original delegate. The adapter never
invokes, suppresses, retries, reorders or replaces that delegate. All C entry
points catch metadata exceptions; failure makes metadata unavailable. No
metadata mutex spans an original call.

## Bounds decided before implementation

| Per context/run | Bound |
| --- | --- |
| Immutable payload pins | 4 |
| Ownership aliases | 32 total, 8 per payload |
| Pending operations | 32 shared between admission and original states |
| Complete new outer receipt | 16 KiB |
| Selected objects | 1 device, 2 textures, 2 heaps |
| Inner retained history | Existing 8 mutations, 8 getters, 2 publications, 4 factories |

There is no eviction. The inner pending token is the calculation mirror of the
same admitted operation; it does not admit 32 additional callbacks. These limits
do not bound total CPU/process allocation or detached copies held by callers.

## Source admission

`d4vr_tcps_creation_enter(context, kind, raw_device)` returns
`{token,selected}`. It atomically consumes a one-shot role owned by the same
thread and epoch before the wrapper reads an IID or description. A thread-local
cookie distinguishes reused thread IDs. Unarmed creation returns `selected=0`,
counts an ignored original ENTRY and does not read its IID, description or
output. The original delegate must still execute unchanged. In particular, a
legal unselected buffer keeps its real ROW_MAJOR layout; it is not converted to
a selected texture.

A selected creation reserves one pending admission. Its original description is
copied by value and checked against the same narrow selected profile. The
wrapper calls `resource_commit` or `heap_commit`, which returns 1 only when that
selection is finalized, before invoking the delegate. This finalization records
the unique original ENTRY in the frozen calculator. Classification itself is
not a second original event. Commit or `creation_reject` consumes the admission
state, and the same external token identifies the original RETURN. Refused
selection never rearms its role. Reentrant/conflicting admission, stale tokens,
changed parent lifetime or a different thread cause a sticky metadata fault.

The parent identity is frozen at reservation and checked again at commit. A
later raw handle lookup cannot substitute a new parent generation. Output is
read only for a finalized selected call with a defined successful original
HRESULT/output. The wrapper keeps the exact HRESULT and every original
argument. Unselected and rejected callbacks can return with raw output zero to
the observer; zero does not claim that the original created no object.

The source-only NEW-device callback describes genuine internal initialization.
The outer internal factory and an existing singleton branch are distinct. An
internal HRESULT is never relabelled as public `D3D12CreateDevice` success, and a
singleton does not mint a generation. Getter and typed callbacks preserve the
frozen historical operand/publication semantics; they do not read descriptor
memory or infer current contents.

## One core context and application controls

The proposed application exports terminate in one core-owned context:

- `D4VR_TypedCopyBeginV1(abi, nonce, nonce_bytes, enabled, run_token)` accepts
  ABI 1, exactly 32 lowercase hexadecimal nonce bytes and an enabled bit.
- `D4VR_TypedCopyExpectV1(run_token, role, sequence)` arms one source role.
- `D4VR_TypedCopySealV1(run_token, sequence, owner_alias, bytes)` seals a receipt.
- `D4VR_TypedCopyReceiptCopyV1(run_token, owner_alias, output, capacity,
  written, required)` copies exact bytes into caller storage.
- `D4VR_TypedCopyReceiptRetainV1(run_token, owner_alias, new_owner_alias)` grants
  a distinct ownership alias to the same payload and C ABI pin.
- `D4VR_TypedCopyReceiptDropV1(run_token, owner_alias)` consumes exactly one alias.

All outputs above are pointers. Status values are `OK=0`, `INVALID=1`,
`DISABLED=2`, `FAULT=3`, `QUOTA=4`, `SMALL=5` and `NOT_READY=6`; they are metadata
statuses, not HRESULTs. A small Copy reports `required`, writes zero bytes and
does not create a lease. It does not append a NUL byte. A successful Retain
returns a token different from every live or previously issued alias in this
control image. Double Drop is refused without consuming another owner's lease.
A raw scalar token copy is not a retained ownership reference. RAII copy or
deepcopy must call Retain; detached byte copies remain caller-owned memory.

`d4vr_tcp_source_context_v1()` is the internal source accessor shared by the
exports and wrappers, never an application fact-input function. The separate
contextual API exists for portable CPU tests. The proposed exports have no
public reset or teardown. Begin is accepted once per context. Wrong run/context,
stale aliases and invalid sequences never select another payload. Controls
accept no raw objects, generations, descriptions, heap addresses, increments,
copy ranges, app phases or clocks.

Seal with a pending admission/original returns `NOT_READY` without a capture
mark or consumed sequence; a role still armed also prevents capture. Full
payload/ownership quotas refuse without capture or eviction. Metadata failure
never prevents the wrapper's original call. Callers must keep the context alive
until all callbacks have returned. Internal CPU-test destruction refuses pending
callbacks; it performs no COM teardown or wait. Retained adapter receipts keep
their own immutable pins. Contextual control destruction also refuses live
ownership aliases.

## New receipt, old calculation preserved

The complete outer JSON has these exact keys:

```text
schema_version, purpose, prototype_only, vendor_built, runtime_executed,
admission, getter_wire, claims
schema_version = 1
purpose = owned_private_translator_source_control_CPU_prototype
prototype_only = true
vendor_built = false
runtime_executed = false
```

`getter_wire` is one complete frozen schema-2 getter envelope nested as an
object. Its old purpose and flags remain unchanged. There is no child raw-byte
SHA claim for a nested object. An external digest may cover the exact new outer
UTF-8 bytes. The new auditor invokes the frozen getter auditor once and retains
its entire result.

`admission` has exactly `selected_reserved`, `selected_committed`, `unselected`,
`rejected`, `pending_admissions`, `pending_originals` and `faulted`. Reserved
counts consumed roles, at most five. Committed counts selected validation
completed before original invocation, regardless of that original's later
HRESULT. Rejected counts consumed roles refused by metadata validation, not
original HRESULT failure. In a sealed receipt both pending counts are zero and
`reserved = committed + rejected`. Unselected is a bounded U64 counter of
creation callbacks outside the selected domain, not a catalogue of their
objects. A rejected selection makes the observer fault sticky. Valid diagnostic
receipts retain negative metadata rather than being promoted.

`claims` contains the following exact 17 fields, all false:

```text
input_facts_attested, opaque_descriptor_decoded,
actual_source_contents_at_copy_proven, current_contents_at_draw_known,
unchanged_since_copy_proven, sampled_resource_GPU_read_proven,
generic_PE_to_Unix_resource_mapping, renderer_owner_known,
exclusive_phase_attribution_proven, global_API_absence_proven,
old_process_or_backing_gate_repaired, public_device_create_result_known,
source_runtime_relation_known, same_run_bind_provenance_known,
getter_to_bind_relation_known, backing_known,
loaded_binary_source_equivalence_proven
```

A private vendor patch, complete generated Device15 ABI, real factory callsite,
source/patch/toolchain recipe and d3d12/core DLL pair still need separate review.
These CPU callbacks and controls do not establish runtime instrumentation,
loaded-code backing, renderer ownership, current descriptor contents or a GPU
read. The earlier source plan's ROW_MAJOR issue is tested through a new source
wrapper revision; no original buffer arguments or old certificates are repaired.

The companion pure Python auditor bounds one complete input at 16 KiB, 4,096
JSON nodes and depth 24, and its complete report at 1 MiB. Its detached CPU-test
batch is at most 64 samples / 1 MiB input and 4 MiB complete output; these batch
copies are not retained native tickets. Healthy accounting requires
`unselected <= inner.source.ignored_calls` and
`selected_committed + unselected <= inner.source.source_calls`. An inner
fault/gap implies outer `admission.faulted`; the reverse is not required because
a later conservative fault can occur after an otherwise healthy inner cutoff.
The complete child audit is retained once, never normalized or truncated.
