# Supplied original typed-copy bind relation

`tools/typed_copy_bind_validate.py` joins supplied app arguments to immutable
source getter and typed-history receipts. It is a pure CPU auditor: it does not
execute an API, inspect a process, attest inputs, or establish runtime binding,
backing, rendering, sampled contents or a GPU read. Previous producers, parsers,
models, fixtures and proofs are unchanged.

The contract and bounds were shared before implementation. `analyze(data)`
accepts UTF-8 bytes, text or an integer-JSON object; `audit_pair` is an alias.
Contradictory framing or joins raise `ValueError`. A valid unavailable source
history produces a negative report that retains the complete child audit.

## Bounds and exact receipts

| Payload | Bound |
| --- | --- |
| Complete outer input | 256 KiB, 8,192 nodes, depth 16 |
| Ordinary string | 256 UTF-8 bytes |
| `wire_text` exception | Exactly two strings, each at most 16 KiB |
| Decoded child | Each child's existing 4,096-node, depth-24 bounds |
| App journal | 384 edges, 192 original/control occurrences |
| Complete output | 1 MiB, including both full child results, journal and contexts |

A `wire_text` string is opaque to the outer node/depth count. Each child is
validated separately. SHA-256 covers the exact UTF-8 bytes returned by the C ABI,
including whitespace; re-encoding a parsed object cannot replace this digest.
The frozen `audit_getter_wire` runs once per wire, twice total, and each entire
result is retained unchanged. Oversized output is refused, never truncated.
These are serialization and parser bounds, not a bound on total process memory.

## Input and output

The schema-1 input has these exact top-level keys:

```text
schema_version, purpose, run_id, source_receipts, app_journal, cases, claims
purpose = supplied_original_typed_copy_bind_CPU_inputs
source_receipts[] = {sequence, receipt_token, wire_text, wire_sha256}
app_journal = {clock_domain, scope, events}
clock_domain = independent_app_original_transition
scope = declared_owned_CPU_mock_subset
```

`run_id` is 32 lowercase hexadecimal characters. Receipt sequences are 1 then 2;
their positive receipt tokens are distinct. There are two case contexts, named
`direct_A_clamp` and `after_bind_B_clamp`. Names identify schedules, not expected
pixel colors. The source has one genuine epoch, three then seven cumulative
mutations, with the first receipt's occurrences preserved in the second.

Every event has the exact keys below. BEFORE and AFTER preserve the same
arguments, API, case and return kind. Ordinals increase at ENTRY; every ENTRY
and RETURN has a distinct increasing app transition.

```text
{ordinal, transition, api, edge, case_ordinal, return_kind,
 args, original_returned, result}
CallRef = {ordinal, entry, return}
Ref = {raw, generation}
```

A void original returns JSON null, never a fabricated HRESULT. The successful
finite HRESULT profile uses `{hresult:0,output:...}`; scalar results use
`{value:...}`. `SealSourceReceipt` is explicitly an observer control with
`{sequence,receipt_token,wire_sha256,wire_bytes}`, not a D3D12 original API.

Each case context retains the following exact keys:

```text
ordinal, name, source_sequence, command, recording_epoch,
selected_heap, selected_index, object_calls, getter_calls, typed_calls,
reset_call, bind_call, root0_call, close_call, seal_call, execute_call,
signal_call, event_call, wait_call, completed_call, map_call, readback
```

`object_calls` has creation CallRefs for device, resource_A, resource_B, cpu_heap,
visible_heap, allocator, command, queue, fence and readback, plus `sampler_heap`
(null or an auxiliary creation CallRef). `getter_calls` has `increment`,
`cpu_heap`, `visible_cpu` and `visible_gpu`. `typed_calls` retains three SRV
CallRefs for case 1 and three new SRV calls plus the late CopySimple for case 2.
All other call fields are exact CallRefs. The declared readback is
`{kind:"CPU_mock_not_GPU_pixels",bytes_hex,sha256}`, exactly 32 supplied bytes.

The output purpose is `supplied_original_typed_copy_bind_history_audit`.
`supplied_argument_relation_known` means that these supplied historical
arguments join; each case preserves its full `getter_audit`, context and
explicit occurrence/getter/typed links. The report retains the app journal and
exact receipt digests. A changed but internally consistent mathematical origin
can remain positive; this auditor contains no A/B pixel oracle.

## Historical joins

The clocks and generation labels of app and source stay separate. Source
internal initialization is not equated with a public app factory HRESULT.
Creation matching requires one candidate with matching class, raw output,
retained description and mapped parent among app creations returned before the
case's Seal. A previous closed creation with the same matching facts is still
ambiguous. No latest-generation or clock-proximity choice resolves it. Future
calls after that case's cutoff cannot retag its retained mapping.

Four distinct app getter CallRefs are cached once and reused by both cases:
SRV increment, CPU heap start, visible CPU start and visible GPU start. Each
joins one eligible source getter by occurrence, parent and scalar value. Equal
repeated getters are ambiguous. Source-local structure-return pointer checks
remain in the child audit; app/source hidden pointer equality is not assumed.
Publication CPU/increment IDs must be the actual source publication IDs.

Creation and use lifetimes must hold through each selected original operation.
The source selected destination is exactly its visible heap, index zero and
original CPU base. Checked CPU/GPU arithmetic and root parameter zero join that
same visible heap. The command list starts recording at epoch zero and has a
setup Close before its first Reset. Reset, heap bind and root0 are the actual
last applicable operations before the selected Close/Execute. Overlapping
operations on the same command occurrence are refused. Close does not freeze
descriptor contents: case 2's copy returns after Close and before Seal/Execute.

The seal's exact digest must fall inside its saved observer-control interval.
Execute is void. One retained producer Execute joins its queue/fence Signal,
the exact event registration and successful Wait, completion and Map. The fence
starts at zero, Signal values increase, and `UINT64_MAX` completion is refused.
A later case Reset cannot invalidate the earlier context. Map only supplies a
pointer: the 32-byte checksum proves self-consistency, not that these bytes
originated from that Map. `readback_bytes_origin_known` remains false.

## Limits of the CPU prototype

The fixture uses ordinary CPU mocks, not D3D12 runtime validation. Its narrow
creation descriptions do not validate omitted heap properties, initial state,
resource alignment, layout, sample quality or a full creation ABI. In particular,
the 32-byte mock buffer's layout-zero description is not proof of a valid real
ROW_MAJOR buffer. The frozen source wrapper's nonzero-layout guard can fault
before its adapter classifies an unselected resource. A future vendor/runtime
route must close that admission separately, or remain unknown; changing original
arguments or bypassing dispatch is not permitted by this prototype.

All input claim flags must be false and stay false in the result:
`input_facts_attested`, `source_runtime_relation_known`,
`public_device_create_result_known`, `current_contents_at_draw_known`,
`unchanged_since_copy_proven`, `sampled_resource_GPU_read_proven`,
`renderer_owner_known`, `backing_known`, `global_API_absence_proven`,
`bind_runtime_proven`, `generic_PE_to_Unix_resource_mapping`,
`exclusive_phase_attribution_proven`, `old_process_or_backing_gate_repaired`,
and `loaded_binary_source_equivalence_proven`. No runtime, rendering or old
process/backing certificate is promoted by this new relation.
