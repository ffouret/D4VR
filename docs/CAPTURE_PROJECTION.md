# Captured images as an explicit successive XR diagnostic

`present::CaptureProjection` now connects the **existing reusable Capture GPU producer** to the deferred `ProjectionSource` protocol. It uses `PoseMailbox` and actual `BoundaryUpdates` state, and returns the producer's images directly. It does not create a second capture pool, redraw the scene, upload substitute pixels, or turn successive game presentations into a common scene snapshot.

This is a reusable native adapter, tested with owned geometry. The regular trace layer now instantiates it in an external-pose/readback diagnostic cycle; see [LAYER_CAPTURE_SOURCE.md](LAYER_CAPTURE_SOURCE.md). **That new binary has not been run in Diablo.** No game image has been submitted to SteamVR/Quest by these changes. Windows compiles the common adapter and exercises the portable receipt guards in CI; native Windows D3D12 game integration remains separate work.

## Ownership and producer integration

The adapter borrows a `Capture` in `gpuStream` mode, `BoundaryUpdates`, one producer/consumer queue and family, and a fixed output extent/format. Construct it after the initial camera buffers exist: it seeds the mailbox above `BoundaryUpdates::packets()` (the fixture deliberately starts at revision 57, independently of XR request 1). The bridge must be the sole camera-request owner while active; independent file-based camera updates cannot compete with it.

The `source()` value supplies request/poll/record/finish and explicitly selects `ProjectionMode::successiveDiagnostic`. One request is active. The left lease is selected first, then a right lease from a later producer sequence and later presentation. Both require exact, uniform copy-submit and selected-draw-interval observations for the original request's remapped GPU packets. Unknown, mixed, stale, wrong-format/extent/family or out-of-order evidence is rejected and retired. Duplicate offers of a held lease do not retire it. There are at most two held leases; Capture's existing four-image / 256 MiB producer bound remains unchanged.

The caller must wire these operations into its **existing** protected queue/producer cycle:

1. `request()` publishes the immutable XR sample through the deferred callback. `prepare()` offers its remapped packet pair to `BoundaryUpdates`; it does not open a boundary or submit/wait for GPU work.
2. At a real admissible presentation boundary, keep the existing `BoundaryUpdates::before` / original submit / `after` tracking across all graphics queues. `acknowledgeQueued()` compares the actual updater's ordered packets against the pending ticket. Echoing a requested packet is insufficient. Acknowledgement is not GPU completion.
3. Use `wantedEye()` to guide subsequent original-render selection. Already recorded commands remain subject to the existing mixed/unknown evidence rules. After each successful original submit, retain the exact protected packet snapshot in `Capture::submitted` and the interval observations, as before.
4. Offer only complete leases returned by `Capture::presentedGpu`. `offer()` consumes a same-queue lease: it either holds it or submits its retirement marker. `foreignQueue` explicitly transfers **no ownership**; its caller must retain and retire that lease on its producer queue later. Queue-family equality alone does not establish synchronization.
5. Observe actual presentation IDs with `observePresent()`. If a left image waits more than `maxPresentGap` presentations for its right image, the source becomes unavailable. This is a bounded presentation-distance policy, not a wall-clock, headtracking or display-latency guarantee.
6. `record()` emits no GPU work and returns the actual Capture images in their producer-established transfer-source layout. The presenter reads them on the same externally serialized queue. `finish()` retires them only after those private reads complete, or after cancellation without consumer submission. Capture owns the retirement fences and only reuses a slot after producer completion, producer-command reset and retirement completion. A failed retirement marks the adapter faulted; it cannot continue with a silently reusable lease.

Capture, BoundaryUpdates and the adapter must outlive the XR session. Finish/cancel the session before destroying the adapter. Its destructor does not invent queue ownership or release images still referenced by a consumer. The caller retains responsibility for completing GPU work before destroying Capture.

## Separate receipts, unchanged coherent requirements

The default coherent mode still uses `coherentProjection` unchanged. The presenter additionally refuses successive receipts in that mode. A diagnostic pair cannot satisfy the coherent guard: its `sceneSequence` values remain **zero**.

The separate `successiveProjection` validator requires the full original session/request identity, predicted time, flags, poses, FOVs, game-view anchor and clipping planes, and the exact remapped packet for each eye. It retains both distinct capture/presentation/lease identities, requires left-before-right order and the configured present gap, and refuses mixed/unknown camera observations. The presenter uses the original XR views and time for composition; it does not replace them with a newer sample when the producer is late.

These receipts prove consistency of the observed selected packet usage only. They do not certify the game's other passes/pixels, common scene time, current pose, scene completeness, dynamic effects, UI compatibility or immersive continuous stereo. A later XR display time can already be stale when a slow source becomes ready. Reports expose `presentation_mode: successive_diagnostic`, `whole_scene_verified: false` and `common_scene_time_known: false`.

## GPU verification

`capture_projection_fixture` renders two triangles at different depths from samples returned by the **mock** `xrLocateViews`, through two fixed BDA camera buffers updated by `BoundaryUpdates`. A separate zero-address pipeline renders independently CPU-transformed vertices. All pixels must match before the original-render image is offered to the adapter. The actual Capture copy/lease is then read by the presenter; the mock runtime independently reads the XR images and compares them to the CPU-transformed render, not to substitute pixel patterns.

Two XR requests, including a 0.025 m head translation and 0.064 m eye separation, produce four distinct images. Source rendering is sequential, with owned stand-ins for swapchain metadata; this fixture does not exercise real WSI or Diablo. Test-only reference readback and waits are outside the adapter/producer hot path.

Controls hold a real producer event/fence, hold consumer GPU reads while verifying both leases remain outstanding, inject swapchain timeouts, reject foreign queue ownership, refuse mixed camera evidence, expire a missing right eye, abandon a one-eye pending source, and verify that coherent mode rejects the successive pair before XR acquisition. Native and Steam Runtime runs load Khronos synchronization validation. `tools/capture_projection_validate.py` produces the numeric proof; no game shaders or private traces belong in the repository.

The new source/receipt code also triggers the previous immediate/deferred/pump regressions. See `docs/proofs/capture-projection-v1.json` for the final tested binaries and measurements.

## Still required for Diablo

The trace layer's diagnostic submit/present/eye-selection wiring now exists, with no XR calls in that driver. Coordinate all real runtime and application queue operations, including runtime reentry, without holding a non-reentrant layer mutex across XR calls or hiding waiting work from BoundaryUpdates. The earlier XR fixture has one serialized caller and explicitly marks XR/runtime access uncovered then covered; it is not evidence that the game's threads are safely coordinated. No background XR worker was added.

Calibrate game VIEW units into the XR head anchor and the actual headset FOV, measure deadlines/cadence, and validate partial-pass behavior with real game pixels. The earlier physical SteamVR validation issues are not resolved by a clean mock runtime. A simultaneous full-scene producer remains needed for the user's intended immersive continuous rendering; this explicit successive mode is only a diagnostic step toward testing the real connection.
