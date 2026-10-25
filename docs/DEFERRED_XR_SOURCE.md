# Deferred XR requests and the camera mailbox

The presenter can now export the view sample received from `xrLocateViews`, let a producer process it over later cooperative turns, and only then request images. The existing immediate `ProjectionSource::record` path remains available. This is an in-process data/producer protocol, not yet a Diablo-to-SteamVR adapter.

## Immutable requests and distinct counters

`PoseRequest` owns the original session/request identity, predicted display time, view-state flags, each eye's position, quaternion and optical FOV, the game-view placement in LOCAL space, and clipping planes. It validates the pose/FOV/affine mapping and derives the two `EyeState` packets. The session identity is process-local, monotonic and non-reused; it is not a persistent or cross-process UUID. A new session must establish its own identity.

The GPU packet revision is separate from the XR request ID. `PoseMailbox` remaps it to the next available diagnostic revision without changing the original sample. Seed the mailbox above the actual current game packet revision; the fixture starts at 2 because its buffers initially contain revision 1. Revisions never wrap into an earlier request. Exhausting the 24-bit packet counter requires a controlled bridge restart.

The initial head pose defines the presenter's anchor, as before. `ProjectionSource::anchorFromGameView` can explicitly map game VIEW coordinates into that anchor's meters, including placement and scale. Identity preserves the private-scene behavior. Neither the default nor the tested placement is a measured Diablo calibration. The retained sample includes the exact resulting affine mapping. These CPU mapping matrices are explicitly aligned for vectorized math.

The mailbox has one pending request and a four-entry queued history, protected by a CPU mutex. `pending()` returns an owned copy. `acknowledge()` requires the producer's actual ordered packet pair and the matching session, request and queue family; merely publishing a requested pair does not acknowledge it. Acknowledgement means **queued**, not GPU-complete. A complete producer image can be associated with a retained request only when its copy and selected-draw-interval observations are both uniform and match that request's exact camera packet. Eviction, cancellation, unknown/mixed evidence or a different family refuses association; it never substitutes the newest pose. A new session clears old history while retaining the increasing packet counter.

No mailbox method owns GPU images, leases or queues, or calls Vulkan/OpenXR. Matching camera observations still does not prove that every game pass/pixel used that pose, or that the eyes share a scene instant.

## Deferred source lifecycle

Supply all three optional `ProjectionSource` callbacks, or none:

1. `request(const ProjectionRequest&)` receives one immutable view sample after `xrWaitFrame`, `xrBeginFrame` and `xrLocateViews`. The presenter retains it, including the original display time and views.
2. `poll(const ProjectionRequest&)` returns pending, ready or unavailable. Pending returns `ProjectionStep::sourcePending`, without acquiring an XR image or recording/resetting the private copy command. Both `advance(true)` and `advance(false)` can poll an existing request; draining does not call `xrWaitFrame` again.
3. Once ready, `record()` supplies the images using the existing strict pose/time/scene receipts. Invalid receipts are still rejected before XR acquisition. Unavailable ends an empty frame.
4. `finish(request, outcome)` is called once after private consumer GPU reads finish, or after cancellation with no such submission. Normal presentation, skipped source, runtime stop, failure and destruction are covered. It must not throw. Ending consumer use does not complete the producer's own writes; the producer still retains its resources through its own fences.

Destruction drains outstanding GPU/runtime work before notifying the source. Callbacks and every XR/Vulkan/runtime call remain caller-serialized with exclusive borrowed-queue access. The mailbox's CPU mutex does not provide queue synchronization. A background XR worker sharing the game's queue is not introduced. Runtime pacing at frame start can still block. Waiting for a producer can make the original predicted time old; there is no VR latency/cadence guarantee or deadline policy here. A reference-space change stops the session instead of reinterpreting retained requests against a changed LOCAL anchor.

## Verification and practical limits

The portable test covers asymmetric FOV, quaternion rotation/inversion, explicit game-view scale/placement, malformed samples, immutable copies, different session/request/packet identities, busy pending requests, exact acknowledgement, cancellation, old-image association, four-entry eviction, session restart and packet-counter exhaustion. A two-thread CPU exchange processes 1,000 requests without losing or retagging one. The test is included in default Linux/Windows CMake and Linux ASan/UBSan checks.

The deferred Vulkan fixture uses real `BoundaryUpdates` commands to update two owned GPU camera buffers from samples returned by the **mock** OpenXR runtime. It independently reads back and compares all 176 bytes per eye before declaring source readiness. Its image producer then renders the existing per-request pixel patterns, which the mock runtime reads back after XR copying. Those patterns verify image transport, **not pose-driven geometry**. The fixture does not use game images or the real headset/runtime.

Controls include a held producer fence, held consumer during stop/destruction, incomplete callback set, source failure/unavailability, stale receipts, reference-space change, swapchain wait timeouts, explicit placement, and native/Steam Runtime runs. Existing immediate-image and cooperative-pump checks are also rerun because their shared presenter changed. See `tools/pose_mailbox_validate.py --help` and `docs/proofs/pose-mailbox-v1.json`.

Still required: bind this request protocol to the real game producer and its queue synchronization, resolve the game's partial scene/temporal evidence with an explicit supported presentation mode, calibrate the view mapping, and validate actual SteamVR/headset behavior and cadence. The previous actual Diablo translation trial remains separate. No Diablo image has been submitted to the Quest by this change.
