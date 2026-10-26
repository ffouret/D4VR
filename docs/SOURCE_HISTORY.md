# Source history at a selected depth COPY

`D4VR_DEPTH_SOURCE_HISTORY=1` adds bounded CPU observations to the existing
[depth COPY readback](DEPTH_COPY_READBACK.md). An exported depth receipt can then
identify the exact observed source-image lifetime, command-buffer recording and
COPY ordinal, with a retained tail of attachment, draw, clear, transfer and
barrier metadata for that source. This helps investigate a local discrepancy
between depth and final color. It does not identify the last GPU writer or
establish why game geometry is missing.

The observer creates no GPU resources and issues no Vulkan commands, waits or
submissions. It allocates bounded CPU metadata and does not reject application
calls. Its new hooks forward the original arguments,
pointers, stage masks, device masks, fences and return values. The independent
COPY producer retains its existing private-resource and fence contract; enabling
history does not grant a new readback route, camera authority or held phase.
Observer initialization failure leaves the application device usable and logs
`source_history_unavailable`. Later observation failures latch `faulted` in the
metadata and preserve original Vulkan forwarding.

## Configuration and private output

Set the flag before device creation. The candidate filter defaults to format
129 (`VK_FORMAT_D24_UNORM_S8_UINT`) and 1707 × 960. Override it with
`D4VR_SOURCE_HISTORY_FORMAT`, `D4VR_SOURCE_HISTORY_WIDTH` and
`D4VR_SOURCE_HISTORY_HEIGHT`. These accept positive decimal values up to 4096;
the format value is the Vulkan enum value. Candidate status means only that the
registered image format and width/height match this filter. It does not mean the
image is an admitted readback source or contains initialized scene depth.

`D4VR_TRACE_DIR` holds the private JSON output. A greater positive sequence in
`source-history.request` requests an aggregate report at an observed presentation
boundary. Device destruction also attempts a final report. There are at most 16
output files per device, named `source-history-PID-DEVICE-N.json`; this file cap
is separate from the eight selected-COPY observations in the aggregate. File
serialization and writes use a dedicated report mutex outside `cameraMutex`.
Reports contain application-derived identifiers and shader fingerprints and
belong in private capture storage, outside the repository.

Each file wraps the `history` aggregate with process/device/report identity,
external request sequence, final-report status and `frame_trace_enabled`.
History enables only the shader-module/pipeline catalogue from
[frame trace](FRAME_TRACE.md). It does not enable full frame capture, descriptor
tracking, mapped-buffer readback, layout/memory capture or shader export.
`D4VR_FRAME_TRACE` remains a separate explicit mode.

## Identity and frozen observations

The depth receipt's `source_history` is either `null` or the five-part reference
`observer_session`, `source_image`, `destination_image`, `recording`, `ordinal`.
These are CPU labels owned by one observer instance. They are not raw Vulkan
handles, another registry's generations, frame identities, GPU timestamps or
cross-session keys. Images with the same format and dimensions remain separate;
destroying/recreating an image handle gives the new image a new identity.

A successful command-buffer begin creates a new recording identity. A common
ordinal orders the observed operations in that recording, including bind,
attachment, draw, clear, COPY, barrier and explicit unknown-route observations.
It is not a count of every Vulkan API call. Multiple regions of one call can
share an ordinal. The selected COPY reference is recorded before the original
COPY and passed by value into its readback receipt. Only a COPY actually selected
by that producer requests a history snapshot; unrelated startup copies do not
consume the eight report slots.

Before the original queue submission, the observer freezes submitted recording
identities, event values and queue/semaphore metadata. Reset, free, re-recording
and later image/view/pipeline destruction cannot retag those retained values.
Every submission occurrence gets its own CPU sequence, including resubmission
of the same recording. The matching `after` call records the original return
value once. The retained tail shares only that occurrence's completion receipt,
so a later CPU return updates its exact pending occurrence without rewriting
events; duplicate returns are counted and ignored. `complete:true` means the
original submission call returned, not that GPU work completed.

A report contains the selected recording only through its target COPY ordinal,
and retains relevant earlier observed commands/submissions. An explicit source
clear after the target COPY in the same recording is excluded from its prefix;
that clear can still appear in an earlier submission occurrence of a later
capture. Incoming image COPY edges retain both image identities when the target
is their destination. An unrelated same-sized image's clear is not relabeled as
source history. This is a CPU traversal/filtering rule, not a proof of GPU
execution order, memory dependencies or a causal last writer across queues.

Draw events retain immutable metadata for the **original** application pipeline
and a separate opaque label for the pipeline actually selected for forwarding.
`pipeline_substituted` makes their difference explicit. A substituted pipeline
does not inherit the original shader fingerprints;
`selected_pipeline_metadata_known` is false in that case, and selected-pipeline
lifetime remains unknown. Original shader metadata carries stage, known status,
FNV fingerprint, byte count and available identifier. It does not include
descriptors, uniform contents or effective dynamic depth-write state.
FNV plus byte count is a correlation key, not a cryptographic proof that two
arbitrary shader definitions are identical.

## Bounds and incompleteness

The default limits retain at most 64 rolling submission occurrences, 2048 events
per recording, 8192 live event nodes and 32,768 snapshot event references. Each
selected COPY retains at most eight relevant occurrences and 512 source-filtered
events. The per-report reservation is also limited to
`live_events / (2 × reports)`: with eight reports and the default node budget,
frozen tails can retain at most half the event-node budget. Rolling occurrences
are reclaimed before a new event is refused, preserving headroom for later
captures. A long-lived recording or other capacity limit can still truncate
observations; this is reported, not treated as a complete history.

Images/views/pipeline labels are capped at 65,536 each, command recordings at
32,768, queues at 64 and semaphore lifetimes at 256. A submission observes at
most 256 batches, 256 commands and 256 semaphore uses. The serialized aggregate
is capped at 8 MiB. Serialization overflow produces a small valid JSON envelope
with `serialization_truncated:true`, not a partial JSON document. Each report
catalogue contains at most 256 original pipelines and 16 shader entries per
pipeline; identifiers are limited to 128 characters with truncation markers.
`event_payload_bytes` excludes allocator and borrowed pipeline-catalogue memory;
it is not a total CPU-memory measurement or the readback producer's GPU budget.

The top-level `candidate_images` list is the live registry at report time, capped
at 64 with an explicit truncation flag. Per-report `images` are frozen historical
metadata used by retained events. A destroyed twin can therefore disappear from
the live list while its earlier lifetime remains in a historical report.
Eviction, capacity overflow, missing prefixes, unknown resources, unsupported
extensions and truncated catalogues remain explicit. A clear/load/store record,
including its exact area and depth bits, does not verify initialized pixels.

Dynamic-rendering depth attachment metadata and explicit depth clears are
observed. Legacy render passes, unexpanded secondary commands, generated work,
BLIT/buffer-to-image routes, unhandled resolve/extension routes, aliasing and
unknown effective write state leave named gaps. Queue identities and submitted
semaphore values/stage scopes are observations, not reconstructed GPU ordering.
Every full report retains `image_memory_aliasing_observed:false`,
`all_write_routes_observed:false`, `gpu_execution_order_known:false` and
`last_gpu_writer_known:false`. It also keeps initialized-pixel, whole-scene,
common-scene-time and XR-pose claims false. Absence from the retained history
cannot prove absence of a write, pass, geometry or culling cause.

## Validation scope

The CPU model tests cover source/twin separation, incoming COPY edges, common
ordinals, partial clears/LOAD/DONT_CARE, original versus substituted metadata,
forced handle reuse, reset/re-record, reverse returns, resubmission, two queue
identities, exact timeline values/scopes, unknown routes, fault and overflow.
A 9000-frame CPU stream retains three late captures under reduced budgets.
These are synthetic CPU inputs and perform no Vulkan dispatch or GPU work.

The forwarding test uses CPU downstream stubs in absent, active and faulted
observer modes. Its added controls pass for 24 command forwards, 18 submission
forwards, six semaphore creation attempts and three destructions, including core
and KHR aliases, original error returns, pointer identity and unchanged masks,
extension data and fences. Catalogue-only mode leaves full frame capture off.

The [owned proof](proofs/source-history-v1.json) records four actual X11/WSI
executions: history disabled/enabled, natively and in Steam Runtime. Each renders
two triangles at different depths over 64 application frames, retains 64
original-depth COPY destination references and 64 original-color references,
and loads Khronos synchronization validation with zero errors. In each enabled
case all 33,554,432 original reference bytes equal the matching disabled control,
for 67,108,864 unchanged bytes across the two comparisons. The fixture owns its
`DeviceWaitIdle` after each presentation; this does not certify arbitrary
asynchronous multiqueue scheduling or general cadence.

The four exported D32 observations join exact same-observer COPY references at
recordings 17/49, ordinal 10. Each target prefix retains seven events and two
geometric draws, with exact original shader fingerprints and submitted semaphore
scopes. Its late source clear is excluded; seven corresponding clears from
earlier occurrences remain visible. The unrelated same-sized twin's clear is
excluded. Each full retained tail has eight occurrences and 91 events, and
honestly reports `history_prefix_missing:true`, `write_route_missing` and
`effective_write_state_unknown`. Two distinct twin lifetimes are observed across
fixture recreation. These GPU executions used different raw twin handles;
forced handle reuse is covered by the separate CPU model control.

A separate synthetic CPU validator control accepts healthy metadata and rejects
ten deliberate alterations: a late clear, a twin/source mix-up, invented GPU
order, wrong COPY destination, wrong original fingerprint, metadata attributed
to a substituted pipeline, altered semaphore scope, conflated lifetimes and
excessive retained occurrences/events. It forbids subprocess creation and
performs zero GPU or WSI executions. These ten controls are not additional GPU
runs or a replay of game pixels.

This owned proof does not establish a game capture or retrospectively join an
older game's image generation. A game report requires its own same-session
COPY/history bridge. The [Diablo depth/color comparison](HELD_DEPTH_COLOR.md)
leaves initialization, all-pass coverage and the wide-field cause unresolved;
CPU source history by itself cannot turn those limits into a causal result.

## Diablo observation

A [separate real-game proof](proofs/diablo-source-history-v1.json) joins three D24
receipts to their exact source-image/view lifetime and common COPY ordinal in
one process. Each retained tail contains 21 depth-attachment scopes, one
`loadOp=CLEAR` declaration at depth 1.0 over 1707 × 960, 20 LOAD declarations,
289/290/289 draw commands and 38 application pipeline identities. There are
67 draw commands without a selected private variant in each tail. Application
pipeline metadata is before private specialization; shader modules may already
be instrumented overrides from the fixed-view bundle.

All three tails explicitly lack earlier history. The aggregate records 42
overflows without a per-cause breakdown, plus unknown extension/effective-write
state. The report is not a complete producer chain or proof of initialization,
executed fragments or GPU order. The retained clear declaration cannot explain
the saturated wide-view pixels by itself. The wide-field holes remain visible.
The game exited through its normal UI and the checked originals/262 exports
were unchanged. No OpenXR or headset was used.
