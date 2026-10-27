# Immutable original getter receipts: CPU prototype

The additive getter adapter retains original CPU/GPU heap-start and descriptor
increment observations by occurrence. It preserves the earlier schema1 source
adapter, wrappers, native transport and history model. It is a CPU prototype
with fake original delegates; it does not patch or build vkd3d, execute a DLL,
or establish a rendering or game resource relationship.

The distinct C ABI uses `d4vr_tcg_*` opaque contexts and receipts. Its source
bridge reuses the frozen original wrapper implementation under bounded aliases;
it does not mix contexts or reinterpret a schema1 receipt as schema2.

The app control plane still supplies only a nonce, one-shot creation-role
admission and a seal sequence. Getter arguments, object generations, output
pointers and returned values come from the source wrappers. No new getter,
QueryInterface, mapped-memory read, descriptor payload read, resource, wait or
submission is added. Metadata failure never suppresses an original delegate.

## Original observations and observer publications

Each admitted getter has its own saved token and common source-clock ENTRY
and RETURN. A record retains its object and parent generation from ENTRY.
CPU/GPU starts return an explicit structure pointer in the pinned source ABI;
increment returns UINT32 and receives a D3D heap-type enum. These are separate
return domains. A matching nonnull output pointer permits copying the defined
scalar value once after the original returns. A mismatching pointer is saved
without reading its payload. A safely observed zero differs from an unknown
value, even though it cannot supply a usable nonzero arithmetic base.

The observer's `heapFacts` publication is an internal metadata operation, not
another D3D12 API call. Its receipt identifies the exact CPU-start and increment
occurrences used at that publication. Repeated equal values retain distinct
occurrence IDs. Export never searches by equality, chooses a recent getter, or
reconstructs a receipt from the current `Object.gpu` value.

## Historical validity and the seal cutoff

Value-known, lifetime-valid-through-RETURN and operand eligibility describe
different properties. Operand eligibility at RETURN does not certify a heap
span or a bind. Publication separately checks the exact base/count/increment
arithmetic before calling the observer's `heapFacts`. If that check fails,
`heap_facts_called` is false and `published` is null; no observer result is
invented. A later Release or diagnostic gap can prevent a new
relation at the seal cutoff without changing a historical original return.
Pending, conflicting, stale, duplicate or ambiguous callbacks never manufacture
an original result or silently switch generations. Release zero closes only
the declared COM occurrence; it does not establish GPU destruction or backing.

The complete serialized receipt is immutable after seal. Later getters, buffer
mutations, releases or context destruction cannot rewrite it. Retained aliases
share a quota pin. The finite profile permits eight getter occurrences, two
heap-fact publications, 32 pending callbacks and four shared tickets per
adapter/run. The complete outer JSON remains at most16KiB, with at most three
selected slot identities and eight typed mutations. Overflow refuses metadata
without eviction. These bounds are not a total CPU memory limit or a promise
that every maximal combination fits the wire budget.

The local final preflight passed 860 Python tests, including 31 new auditor
tests, and all five new CMake controls. The independent native suite covers
73 cases and 1,194 assertions; its 69 wires include 65 model-consumed snapshots
and 29 known mathematical histories. The separate real-wrapper/C ABI integration
uses fake originals and has 13 cases, 951 assertions and 17 wires. Compile-off
forwarding has 60 assertions. These counts overlap and are not a combined
coverage total. Local Windows declaration objects were compiled only.

A controlled local mutex fault exposed capture returning no receipt while
leaving the new context healthy. The final adapter latches a diagnostic fault
on capture failure. Ordinary four-ticket quota refusals still leave the clock
and context unchanged. Earlier sources and proof checkpoints are preserved;
this correction involved no vendor, Wine or GPU execution. Native sanitizer
coverage is checked separately by CI, not claimed for local runs or Python.

The public summary is [immutable-getter-receipts-v1.json](proofs/immutable-getter-receipts-v1.json).
The parser reviewer authored the bridge and earlier transport/model; the parser
author authored the native countertests. These reviews are not external
certification. New C integration processes also have different actual stack
pointers: their raw wires are independently audited, not claimed byte-identical.

## What this leaves open

The new pure auditor checks only supplied CPU records and exact occurrence
links. It does not attest those inputs. Source runtime, public factory results,
same-run bind provenance, current descriptor contents, unchanged bytes, GPU
sampled resources, renderer identity, module backing and global API absence
remain unknown or false.

A future bind join needs an independent original command context with heap and
root-table arguments, command-list generation and recording epoch, Close,
late-copy, Execute, fence and readback occurrences. App and source clocks remain
separate namespaces. This receipt alone authorizes no rendering claim.

Before a private vendor build, review the actual source insertions, complete
tree and recursive submodules, patch series, toolchain, generated files,
dependencies, ABI exports and compatible private D3D12/Core pair. Existing
pixels and disk hashes do not attest that future binary. No new game or headset
run is required for this CPU increment.
