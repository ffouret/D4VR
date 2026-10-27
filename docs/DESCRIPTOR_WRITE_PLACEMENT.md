# Owned descriptor write placement

This control tests a narrow CPU relation: whether one returned, typed `vkGetDescriptorEXT` output occupies a uniquely selected descriptor-buffer slot at that query's return. It combines a bounded Python model, caller-owned snapshots, and six owned Vulkan cases. The model is not a live layer observer. A positive result establishes neither descriptor contents at draw time nor a GPU resource read.

The native and Steam Runtime executions passed on the first attempt: 12 readouts, 176,947,200 bytes checked against an independent exact BGRA oracle, and zero Khronos validation errors. The corresponding native/runtime files were byte-identical. The [public proof](proofs/descriptor-write-placement-v1.json) records CPU preflight and physical results separately.

## Relation at query return

`tools/descriptor_write_placement.py` accepts one exact-schema `owned_fixture` snapshot per returned query. It receives explicit typed input, output pointer and bytes, resource generations, allocation/buffer bindings, maps, queried device addresses, layout sizes and binding offsets, descriptor properties, and the selection established before the query. It reads no address or mapped memory itself. The snapshot's completeness and lifetime facts are supplied by its caller; the model cannot attest them.

The model checks a unique chain from the output pointer through its live map and allocation to the bound buffer and selected set/binding/array element. In allocation coordinates, the selected byte is:

```text
buffer memory binding offset
+ (selected bound address - buffer device address)
+ selected set offset
+ queried binding offset
+ array index * descriptor stride
```

The pointer must describe that same complete typed output span through the map's allocation offset and CPU pointer. Checked unsigned arithmetic, allocation/buffer/layout generations, matching before/return epochs, alignment, ranges, layout revisions and prior selection ordinals are required. An array element's stride is not replaced with descriptor-buffer set alignment. Mutable bindings use the maximum admitted type size; robust buffer types use the relevant robust size when that feature is known enabled.

Overlapping buffer bindings or device-address aliases prevent a unique placement result, including an overlapping buffer without descriptor usage. Disjoint bindings in one allocation are allowed. Failed or incomplete live bindings, simultaneous live generations sharing a raw handle, multiple live maps of one allocation, missing facts, unknown types/chains, unsupported layouts, overflow and exhausted bounds remain unavailable. Opaque output equality is never used to infer a resource identity.

Every model result and receipt fixes these four facets to false:

- `current_contents_at_draw_known`
- `descriptor_unchanged_until_execution`
- `descriptor_dynamically_accessed`
- `sampled_resource_GPU_read_proven`

## Bounded immutable receipts

`Registry.reserve(sequence)` creates a ticket only when its shared quota permits. `Ticket.record(snapshot)` appends an ordered result, and `Ticket.receipt()` seals the ticket into bounded immutable JSON. Copies retain the same quota pin. Exporting a receipt performs no current-catalogue lookup, and later tickets cannot replace or evict it.

| Bound | Limit |
| --- | ---: |
| Live tickets, including externally held receipts | 4 |
| Selected logical slots per ticket | 3 |
| Returned writes per ticket | 8 |
| Serialized receipt bytes | 16,384 |
| Snapshot input bytes | 16,384 |
| Snapshot nodes / nesting depth | 4,096 / 12 |
| Allocations / maps / buffers per snapshot | 8 each |
| Layouts / bindings per layout | 3 / 32 |
| Mutable binding types | 8 |
| One returned output | 256 bytes |

Bounds refuse observer metadata, never an application's Vulkan call. In this milestone there is no application hook to reject, alter or delay. The model operates on supplied snapshots only.

These are serialized payload limits. Detached JSON exports and Python allocator overhead are not included in a claimed global memory ceiling.

## Six owned Vulkan cases

`native/tests/descriptor_placement_gpu.cpp` reuses the unchanged owned-resource setup and the same original vertex and fragment modules for every case. It uses two known 8×8 UNORM textures, nearest clamp/repeat samplers, a 1,280-byte UBO with a fixed one-texel offset, zeroed 28-byte push constants, and a full 2560×1440 UNORM output. The fixture uses descriptor buffers, not a shader or bundle modification.

| Case | Query destination and subsequent owned operation | Expected sampled texture / sampler |
| --- | --- | --- |
| `direct_A_clamp` | Typed A query writes directly to selected image slot | A / clamp |
| `direct_B_clamp` | Typed B query writes directly to selected image slot | B / clamp |
| `scratch_B_clamp` | Typed B query writes to scratch, then an owned CPU copy fills the selected slot | B / clamp |
| `identical_hook_A_clamp` | Typed A query writes directly to selected image slot | A / clamp |
| `identical_hook_overwrite_B_clamp` | Same saved query/binding window as the previous case; a CPU copy overwrites the image slot with B before submit | B / clamp |
| `direct_B_repeat` | Typed B query writes directly to selected image slot | B / repeat |

Each saved observation window contains the actual descriptor-buffer bind, offsets for sets 0/1 and then set 3, followed by image and sampler queries. The image and sampler snapshots are captured at their returns. Their ordinals are local CPU-call positions within the case, not recording, submission, present or GPU-order identities. Layout/allocation/map facts are explicitly an owned fixture bootstrap. The snapshots include all five owned live buffers and their allocations/maps.

Scratch-copy and hidden-copy groundtruth are kept outside the model input. The hidden copy occurs after command-buffer completion and immediately before the original submit, with no intervening Vulkan callback. Its source is the vector most recently written by the earlier `scratch_B_clamp` query. The auditor joins those actual bytes; it does not assume equal typed queries produce canonical opaque bytes. Image and sampler slot bytes are saved before submit, with submit/fence success recorded afterward.

The scratch case must remain unknown for image placement at query return, even though its final pixels can match direct B. Conversely, both identical hook windows must retain the same positive A placement at query return, even though one later samples B. This is the decisive limit of observing only returned queries and binding commands.

## Independent audit

`tools/descriptor_placement_validate.py` joins snapshots to original call operands and creation tables, including generation, binding offset, map span, queried address and layout metadata. It checks exact case filenames, hashes, push/UBO bytes, a fixed validation log, and five preparation queries kept outside the model. Groundtruth copy integrity is checked separately from placement analysis.

The pixel oracle derives the complete BGRA output from known texture formulas and rational pixel centers, including the original vertical coordinate flip, fixed offset and nearest clamp/repeat addressing. It uses no fixture-produced reference image. It requires exact agreement in all four channels, a sampler witness, and different B/A bytes for the two identical observation windows. The latter count may come from B alone; alpha need not differ.

CPU preflight included 30 model tests, 12 auditor tests, 33 independent model adversaries, and 21 additional checks of the auditor, receipt retention and a complete six-case synthetic dataset. All 495 portable Python tests passed at source freeze. The fixture compiled with strict Linux warnings and as a Windows object; the Windows object was not executed. The independent synthetic pixel generator used `Fraction` coordinates without calling the author's reference function. Four retained receipt copies held the quota while 80 new reservations were refused, then released it correctly. A deepcopy quota bug, six lifetime/alias contradictions, and missing original-argument joins were corrected before any physical execution. Those corrections are preflight validation, not an application or GPU rendering repair.

## Physical results

Each environment completed seven submissions, six draws and six readouts, with two texture uploads and twelve query-return snapshots in the six case windows. Five preparation queries per environment remained outside the model. All twelve sealed receipts were complete. Across the two environments, 22 queries had positive placement at return; the two scratch image queries correctly remained unknown. All four stronger facets stayed false for every query and receipt.

The same saved query/binding windows produced different final pixels after the unobserved CPU overwrite. Their B/A comparison differed at 3,628,800 byte positions per environment, counted once for each A/B pair. Direct B, scratch-copy B and hidden-overwrite B pixels were byte-identical; repeat B differed from clamp B. All four channels matched the independently computed original-shader reference exactly.

The two environments produced six corresponding file pairs, comparing 88,473,600 bytes once for native/runtime equality. The full CPU-reference comparisons covered all 176,947,200 output bytes. Validation counters included teardown: zero errors and eight loader warnings in total, three native and five under Steam Runtime. The warnings concerned explicit layer loading, duplicate runtime layers and a skipped unavailable ICD. Synchronization validation was enabled.

The parent runner exited zero after 20.198708 seconds of host elapsed time. All nine physical and final source fingerprints matched. No GPU retry, code repair, acceptance-policy change, game run or headset action was needed.

A separate independent CPU review checked every physical BGRA byte using a reference fixed before execution, with `Fraction` coordinates and explicit texture formulas. It called none of the author's pixel, texture or reference functions. It reused the frozen placement model for metadata, which is a distinct role from the independent pixel calculation. Both saved environment audits were reconstructed as exactly equal dictionaries. The review verified 82 physical files, nine sources and two original modules unchanged before and after; four one-byte corruptions in B, G, R and A, made only in memory, were refused.

The fixture author also replayed both saved audits and refused 14 metadata/copy-integrity falsifications made in memory. That is corroboration by the author of this owned control, not independent observation of game arguments. Both reviews launched no processes or GPU/UI actions and changed no physical output or saved audit. Their separate evidence hashes are retained in the public proof.

## Limits

The model's result is an at-query-return relation over caller-owned facts. Unobserved CPU stores and ordinary copies can invalidate any inference about later bytes. Buffer binds and offsets do not freeze descriptor contents. Exact opaque bytes, dimensions or a typed query alone cannot establish which descriptor a shader actually accesses.

No live layer observer, translated D3D12 copy path, game input, new game run, numerical game UV admission, game rectangle cause, rendering repair, headset trial or XR submission is established here. The earlier game observation remains unchanged. A future live integration needs its own lifetime, write/copy and visibility evidence rather than promoting this owned fixture's assumptions.
