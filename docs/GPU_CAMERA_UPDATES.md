# Bounded GPU camera updates

The optional `D4VR_GPU_CAMERA_UPDATES=1` sequential diagnostic can change the two camera packets at their existing GPU addresses. Its input is an atomic-renamed `camera-pair.bin` in `D4VR_SEQUENTIAL_CAPTURE_DIR`, containing two consecutive 176-byte `EyeState` records (left, right), with the same increasing sequence. This is a developer diagnostic, not a SteamVR pose source or a playable VR mode. Without the option, camera controls remain immutable.

`Control::updateTarget()` exposes an explicitly enabled transfer destination. `recordPacketUpdate()` records two buffer barriers and `vkCmdUpdateBuffer` outside rendering. Vulkan copies the small payload when recording; there is no host write to a packet being read by a shader. Buffer addresses and graphics pipelines remain fixed. Each packet contains the view and projection matrices, its eye and sequence; invalid ABI, queue family, alignment, size, usage or eye identity is refused before recording commands.

## Multiple graphics queues

The game requests five graphics queues. A barrier on one queue alone cannot protect readers on another. `BoundaryUpdates` therefore uses this conservative policy:

* Require one graphics family, one to eight unprotected graphics queues, and the timeline semaphore feature already enabled by the application. Register every requested graphics queue; a missing queue prevents updates.
* Track every graphics `QueueSubmit`, `QueueSubmit2` and `QueueBindSparse` with a private completion marker. Keep four marker fences per queue, one update command buffer/pool/fence and one timeline semaphore. Saturating markers leaves the queue uncovered; a later marker can recover coverage. No per-update Vulkan resource allocation is needed.
* At the first submission after a successful presentation boundary, apply a newer pair only if all tracked work on all graphics queues is proven complete and the previous update command is complete. Otherwise keep the current pair for that interval and try at a later boundary. No host GPU wait or queue-idle call occurs in the update path.
* Order the GPU packet write before vertex shader readers. Other graphics queues each wait on that update's private timeline value, even if the CPU already observed its fence complete. Do not impose an ordering between the application's unrelated queues or modify its submission arguments/semaphores.
* Serialize `before + original submission + after` across the registered graphics queues. Presentation is marked uncovered before the original call and covered on return, without keeping the global submission mutex across the original present call. Queue external synchronization is still the application's responsibility.

All methods require caller serialization. This is not a claim that every call on a game render thread is nonblocking: the original driver calls and the diagnostic mutex can still take time. The conservative idle requirement can defer a pose indefinitely in a continuously busy game. A private submission failure stops subsequent tracked submissions rather than forwarding shader readers without their required dependency. Device teardown completes private work before releasing the updater. Arbitrary hidden queue work, multiple graphics families, protected queues and other readers of these buffers remain unsupported.

## Provenance and limits

The update log records the exact **queued packet sequence**, not the pose of a completed game frame. Captures can arrive later and the original renderer has multiple passes and reused resources. No current camera sequence is assigned retroactively to an older captured image. Capture now carries separate value-owned copy-submit and selected-draw-interval packet observations; see [camera packet evidence](CAMERA_PROVENANCE.md). Whole-scene and actual XR-pose provenance remain unavailable. Both views are still successive; none is sent to OpenXR by this diagnostic.

`FrameOrder` separately rejects duplicate or retrograde capture/presentation IDs per eye. It keeps two fixed high-water marks, preserves the producer's IDs and retires discarded GPU leases too. It does not establish simultaneous stereo, a current XR pose, or a common scene instant.

## Verification

`eye_update_queue_ownership` is a driver-free C++ test, also built on Windows and under Linux ASan/UBSan in CI. It checks missing queues, duplicate/mismatched requests, pending readers/writers, marker backpressure, uncovered external queue operations, recovery, bounded allocation, cleanup and late frame rejection.

`d4vr_eye_updates_fixture` renders owned geometry using five actual graphics queues on the RTX 5090. Nine pairs of camera matrices include translation, rotation and projection changes. Every eye image is compared byte for byte with independently transformed CPU vertices rendered by a separate zero-address pipeline. A real event holds a reader pending on another queue; the updater keeps the old pair, returns without a GPU wait and recovers only at a later boundary. The fixture uses host waits to inspect its results; those waits are not part of the updater.

The real X11 WSI fixture also exercises the complete loader layer, three live packet revisions, five registered queues, unchanged application draw counts, geometric displacements matching their depth, and restored original pixels. Native and Steam Runtime runs load Khronos synchronization validation and reject any validation error. `tools/eye_updates_validate.py --help` lists the reproducible artifact paths. These fixtures use synthetic geometry; they are not screenshots or proof of Diablo running in VR.

## First actual Diablo camera-update trial

`docs/proofs/diablo-camera-updates-v1.json` records a stationary, no-headset trial of source commit `e8b2702`. Three 2560×1440 images use the same left eye: initial matrices, a requested lateral translation of +0.128 view units, and restored matrices. Of 28 contrasted terrain patches, 27 pass the correspondence criteria; all 27 move horizontally by 6–16 pixels and return to their original coordinates. This demonstrates a camera update affecting real terrain without relaunching Diablo. It does not establish complete scene coverage, head tracking, simultaneous eyes or SteamVR delivery.

The game registered five graphics queues; this session required zero cross-queue camera waits, so the separate multiqueue fixtures remain the evidence for concurrent readers. Two request-to-log polling observations were about 0.032 and 0.281 seconds; these are not display latency measurements and provide no VR cadence guarantee. Busy and uncovered boundaries were skipped. The final sampled marker backpressure count was high (440,006), with resources kept bounded. Diagnostic overhead was not benchmarked.

The initial capture predates a private monitoring filename-match correction; it uses the same view matrices as the restored capture. Three pixel exports and the updater log support this comparison; they are not per-image applied-pose receipts. All game/launcher processes exited and original game/vendor files plus 262 named exports matched the pre-launch audit. No in-game Vulkan validation or device-destroy trace was recorded. Linux/Windows CI run 120, including the portable updater tests and Linux ASan/UBSan, passed. Game images and shaders remain private; only derived measurements are public.
