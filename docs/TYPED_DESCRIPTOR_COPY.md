# Typed D3D12 descriptor history

This pure CPU model consumes supplied original API facts for a small owned
fixture. It is not a live game observer, descriptor decoder, translator hook or
rendering certificate. `input_facts_attested` is always false.

The domain is two 8×8 Texture2D resources with DXGI format28, one CPU-only
two-slot SRV heap and one shader-visible one-slot SRV heap on one device.
Resolution checks the supplied original heap base, increment, index, generation
and uint64 arithmetic. CPU descriptor handles are opaque arithmetic tokens;
the model never reads a handle, address, descriptor payload or mapped memory.

## API

`tools/typed_descriptor_copy.py` provides:

- `analyze(snapshot)`: detached JSON from an exact bounded dictionary, UTF-8
  string or bytes. Malformed framing raises ValueError; well-framed unsupported,
  incomplete or conflicting history returns unknown with reasons.
- `Registry.reserve(sequence)`: a Ticket, or None for full quota or a duplicate
  live sequence. Even an empty reservation pins quota.
- `Ticket.record(snapshot)`: one successful seal. A malformed, oversized or
  excessive-slot capture returns false without eviction. An unsealed ticket may
  retry corrected input.
- `Ticket.receipt()`: None before sealing, otherwise an immutable shared
  receipt. `serialized()` includes the sequence and full report; `report()`
  returns a detached JSON copy; `sha256` hashes the serialized ASCII JSON.
  Copies and deep copies of Ticket and Receipt share the original quota pin.

The exact schema is exercised by the independent builders in
`tests/test_typed_descriptor_copy.py`. Top-level fields are schema_version,
purpose, run_id, domain, at_transition, complete, gap, device, resources, heaps,
events and selected_destination. The nonce and completeness assertion are
supplied facts, not an attestation of runtime observation. Transition numbers
order CPU observations, never GPU work.

## Original calls and historical versions

Every event names its original API: CreateShaderResourceView,
CopyDescriptorsSimple or CopyDescriptors. BEFORE descriptions, resources,
handles, range nullness and operands are retained by value. All three APIs
return void: input framing requires return_kind="void" and result=null.
No HRESULT or success return is manufactured.

Initial copy admission is one CPU source slot to one visible destination slot.
NULL range-size arrays mean one descriptor per range and keep their nullness.
Unsupported descriptions, larger/mismatched copies, partials and aliases do
not acquire a complete unit-copy lineage.

Replay freezes the source's last observed typed version at copy ENTRY.
RETURN may publish a destination history version only if device, heap and
originating resource occurrences remain valid without a conflicting mutation.
A later source write cannot retag an earlier completed copy. A future Release
or overlapping write may prevent complete lineage while ENTRY history remains
as a diagnostic. Generations increase in creation-entry order with gaps
permitted. Reuse waits until the old original Release RETURN; mutation
admission conservatively stops at Release ENTRY. Pending/reversed/conflicting
mutations and unknown writes remain unknown, without a current-state lookup.

selected_history_known describes finite API history only.
original_typed_copy_call_returned additionally requires a returned admitted
copy; a direct CreateSRV is not a copy. Silent overwrites can produce identical
observed history with different hidden contents. Actual source contents at
copy, current contents at draw, unchanged contents, GPU reads, opaque decoding,
renderer ownership, generic PE/Unix conversion, exclusive phase, global
absence and repaired backing/process gates therefore remain false.

No bind, Close, Execute, fence or pixel observations are joined by this schema.
It cannot identify the game's sampled image.

## Budgets

Four shared live tickets are permitted. Each ticket retains at most three slot
identities, eight mutation records and16KiB of complete ASCII JSON including
its sequence envelope. Four full tickets imply at most32mutations,
12slot occurrences and64KiB serialized payload. These are not total Python-heap
or transient-allocation bounds. Retained receipts survive deletion of their
Ticket; no held value is evicted or rewritten.

Inputs also limit4096nodes, depth14, strings256characters, ordinary arrays16
elements and objects32keys; resource/heap/event arrays have smaller explicit
caps. Unknown reports may retain bounded unsupported operands; a Ticket
refuses reports mentioning more than three slot identities.

## Validation and next boundary

The permanent suite has37 independent CPU tests. Author controls and
adversarial review also cover entry snapshots, pending/reversed/disjoint
mutations, generations, NULL ranges, void returns, arithmetic, exact wire
boundaries and shared retention across80refusals and80successful recycles.
Review counts overlap and must not be summed as distinct tests.

Two synthetic false positives were corrected before freeze: decreasing
creation generations and raw-object reuse during a pending Release.
Counterexamples and historical checkpoints remain private. No old native
observer, renderer, shader, helper, game bundle or strict audit changed.
No new Wine, Vulkan, GPU, game or headset run was needed.

Next, prepare a typed receipt producer at the reviewed original D3D12 boundary
with same-run bind/Close/Execute/fence/readback joins. Only then run two owned
cases: direct A and a single copy of B after Close before Execute. The old
eight-case pixel proof already established that visual effect; repeating it
without new transport would add no provenance evidence.
