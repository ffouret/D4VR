# Depth COPY observations under a held diagnostic camera

This opt-in diagnostic composes the bounded [original-COPY depth reader](DEPTH_COPY_READBACK.md) with the external pose driver, [monoscopic view hold](FIXED_VIEW_HOLD.md) and the existing original-presentation color capture. Its purpose is to compare depth and color under an observed diagnostic mapping before interpreting the wide-field holes. It does not establish initialized scene depth, complete scene coverage, a common depth/color instant, stereo rendering or XR delivery.

The composition now passes four owned X11/WSI/loader executions: a held-color control and the combined mode, each natively and in Steam Runtime. [held-depth-color-v1.json](proofs/held-depth-color-v1.json) records their exact source/binary fingerprints and results. A subsequent [actual Diablo trial](proofs/diablo-held-depth-color-v1.json) now supplies three D24 readouts and six successive monoscopic color pairs at reference/wide/return mappings. The earlier [actual original-camera D24 depth export](proofs/diablo-depth-copy-v1.json) remains a separate observation made with its historical binary.

## Explicit admission

`D4VR_HELD_DEPTH_COLOR_DIAGNOSTIC=1` requires all of the following: `D4VR_DEPTH_COPY_READBACK_DIAGNOSTIC=1`, `D4VR_POSE_CAPTURE_DIAGNOSTIC=1`, `D4VR_DIAGNOSTIC_VIEW_HOLD=1`, `D4VR_GPU_CAMERA_UPDATES=1`, both eye-packet files, a nonempty private `D4VR_TRACE_DIR` and a nonempty `D4VR_SEQUENTIAL_CAPTURE_DIR`. Depth audit, the older EndRendering reader, cooperative XR, XR binding, replay, GPU-capture dry run and a single-eye override are excluded. The isolated original-camera COPY mode retains its own admission policy.

The explicit composition initializes its COPY producer, paired pipelines, validated packets, camera-update controller and presentation producer together. A configuration or component failure returns initialization failure and a null device handle, cleaning up any components and created device. A failure cannot silently leave a reduced diagnostic running. The color consumer is initialized lazily from a known presentation chain; no depth request is admitted before that consumer exists and reports a held monoscopic mapping with no active/pending color request, stop or fault. Rejected request sequences are consumed and logged so an unchanged rejected file cannot later become a capture under another mapping. A later request uses a larger sequence.

The portable `held_depth_mode.hpp` predicates check configuration, complete components and current request state. They perform no Vulkan work and authorize neither completion nor a scene claim. Existing API, device-feature, image, recording, family and original-COPY restrictions still apply.

## Depth and camera provenance

The added depth-aspect image-to-buffer COPY remains immediately before the admitted original image-to-image COPY. It reads a subset of the original source aspects at COPY stage, with the same footprint and layout. Its barriers name only the private mapped buffer. The original destination is not accessed by the private command, and its mutable-format identity does not relax source admission.

The request freezes an exact eye-zero held packet by value. The recording's selected eye is retained separately. Before each original submission, the layer snapshots the protected camera packets and a shared capture-submission identity; the COPY producer retains the resulting observation only if that original submission succeeds for the current epoch. Unknown provenance, a different packet or a different recording eye cannot be repaired by a later request or export-time lookup.

A packet observed at the COPY submission describes that submission's diagnostic packet. It does not prove which packet produced every source-depth pixel, identify all contributing passes, establish initialization or close the resource history. Depth and color may carry matching diagnostic mapping observations without sharing a scene instant. Their recording, original submission, epoch and color-presentation identities must remain distinct.

## Completion, stop and storage

Depth completion uses the private fence passed in place of a null fence on the original submission. An application-owned fence remains untouched and disables the affected depth export. Readout still requires successful original submission and current-epoch `GetFenceStatus`; every resubmission invalidates the previous pending depth observation before forwarding. The depth path adds no queue marker or GPU wait.

The color path keeps its existing downstream producer markers, leases, consumer copy and retirement markers. Those separate submissions are not depth-completion evidence. No claim that the combined diagnostic adds zero queue submissions follows from the depth reader's narrower contract.

A terminal stop in either component, or a camera/consumer fault, prevents new depth admission and unfinished work from becoming a receipt. Reset/free removes recording lookup while resources already named by submitted commands remain alive. Teardown completes actual device work before releasing inserted buffers, fences, color images and camera packets. Already completed CPU exports retain their frozen provenance.

GPU image/readback allocation caps are additive: at most 128 MiB for retained depth buffers, 256 MiB for the color producer and 128 MiB for its readback consumer, totaling 512 MiB for those resources. Bounded CPU copies require additional memory and are not included in that GPU allocation total. Device/driver memory overhead and camera/pipeline objects are separate. Synchronous allocation, bounded CPU copies and lock contention can delay the application; no VR cadence is established.

Color delivery is staged under camera protection and moved to a local immutable export before releasing that protection. Disk writes for depth, color and receipts occur outside `cameraMutex`. The COPY producer's own mutex still excludes resubmission while checking completion and copying mapped words. Export code must not substitute the current pose request for the observation already stored with an export.

## Owned verification and its limits

The portable admission test covers seven missing options, seven excluded modes, five incomplete components and seven unsafe request states. The producer controls additionally freeze recording eyes, request packets and successful original-submission observations by value; wrong/unknown packets, changed revisions or matrices, absent shared submission identity, resubmission epochs, late returns and reset/re-record cannot retag a captured observation.

The four completed executions render two owned triangles at different depths over 64 application frames. Each run independently saves 64 original-depth COPY destination references and 64 original-color references. Every enabled run preserves all 33,554,432 reference bytes from its matching disabled control: **67,108,864 unchanged original bytes** across native and Runtime comparisons. All four runs load Khronos synchronization validation and report zero errors. The fixtures keep original COPY/BLIT signal scopes, device mask zero and null submission fences; their own `DeviceWaitIdle` after every presentation is an explicit application dependency, not an injected wait or a general cadence result.

Each enabled run exports two D32 images, requests 3 and 5, at 256 × 256 and 262,144 bytes each. They exactly equal the independent original COPY destination. Requests 1, 2, 4 and 6 are refused respectively before consumer initialization, during each of the two active color requests, and after terminal stop. The same rejected request does not become accepted when the state changes. The two successive monoscopic color pairs per run exactly equal their retained original application-color references and preserve their actual present/capture identities. Their source images remain successive, with differing presentation gaps allowed.

The depth observations and matching color phases retain the same exact **176-byte eye-zero packet**, including the original packet revision and mapping. Each depth receipt also retains its shared capture-submission identity alongside its separate COPY-submission counter; color sources retain their own submission/presentation identities. These identities are not merged. The controlled packet change moves the nearer triangle about −14.28145 pixels and the farther triangle about −7.12310 pixels horizontally. Analytic D32 references are 0.95095095 and 0.975975975, with maximum allowed absolute error 4 × 10⁻⁷; background depth is 1. This is geometric depth, not a clear-only test or a metric-depth claim.

A separate CPU replay reruns the validator against disposable copies of the four recorded cases. The healthy data passes; 16 deliberate alterations are rejected, covering private/clear-only depth, packet bytes, recording eye, submission identity/order, refused requests, color bytes, invented common time, fence counts, validation receipts/errors and original COPY scope. The replay uses the exact captured validator and forbids actual subprocess creation. It performs zero GPU operations and does not constitute 16 additional GPU controls or modify the source captures.

The same layer binary also passes 12 isolated-COPY regression cases, with their original geometry/direct/loader/WSI controls, and eight held-color cases with calibrated packets checked in both shader stages. The latter cover native/Steam Runtime, short runs, foreign-queue acquisition and cancellation; ten color pairs preserve 5,242,880 original bytes. Their legacy sequential-capture controls comprise exactly three cases: native, disabled and Steam Runtime. Khronos synchronization validation reports zero errors in these regression runs. They preserve those components' established scopes and do not add combined-mode multiqueue or future-batch evidence.

The combined fixture does not separately exercise a future-signal batch or certify arbitrary asynchronous multiqueue applications. Those cases in the isolated COPY proof remain separate evidence. The combined run's stop and final teardown retain bounded ownership with no outstanding color leases; broader scheduling, all contributing passes and source-depth initialization in a game remain unproved.

The actual Diablo trial retains 19,664,640 depth bytes at 1707 × 960. Its three measured preceding holds span 5,413/5,398/5,418 host milliseconds over 780/779/782 uniform selected-draw intervals. Each depth packet exactly matches its preceding warm color eye zero (revisions 2/4/6); the later measurement color uses the same mapping with a new revision (3/5/7). These are successive observations, not common-time image pairs. Original COPY and shared capture counters happen to be numerically equal in this trial; their meanings are still distinct from color presentation/capture identities. Entry and exit use only normal UI under the user’s explicit autonomous authorization. The launcher exits zero, all owned processes exit, and originals plus 262 exports remain unchanged. No in-game Khronos or original-pixel equality is claimed.

No physical SteamVR result is established here. The wide-field coverage cause, simultaneous stereo, continuous rendering, native Windows D3D12/VR integration and the previously observed physical SteamVR DRM/WAW issues remain unresolved. No Diablo image has reached the Quest.

## Actual-game comparison: local discrepancies, cause still open

The CPU comparison admits 48 of 140 candidate terrain patches jointly in color,
raw depth, optical mapping and return; all 48 have zero depth-position residual.
The same diagnostic thresholds admit 50 color patches and 108 depth patches
separately. These are filtered correspondences, not a scene-completeness score;
a rejected patch does not prove absent geometry. The color comparison accounts
for the reduction in optical sampling bandwidth. Thresholds and the unmodified
projection control are retained in the game proof.

The wide depth readout contains 196,073 maximum D24 words (11.965% of its
footprint), versus none at reference/return. Region A, outside the old view's
projected footprint, has 40,560/40,560 saturated words and nearly uniform color.
No baseline terrain is established there, so saturation cannot prove culling.
Region B, inside that footprint, is dark but retains 4,243 distinct depth values
across 7,659 samples, without zero or maximum words. Its depth differs from the
optically mapped reference by a median 11,924.6 raw words; it does not establish
correct unchanged terrain depth under missing final color. Neither region alone
isolates an upstream or downstream cause.

At return, 1,596,682 of 1,604,832 nonzero words (99.492%) are exactly restored.
The same 33,888 zero-word mask appears throughout and overlaps HUD-like
rectangles; it is not classified as missing terrain. A separate 294-check CPU
audit confirms sizes, D24 padding, immutable packet/epoch receipts, holds and
successful exit. This audit performs no GPU work. Source-producing passes,
initialization, all-pass camera coverage and common scene time remain unknown.
The next useful step is to identify the producing pass/history of the local depth
discrepancy inside the old view, rather than repeat these unchanged captures.
