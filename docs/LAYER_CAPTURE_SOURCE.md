# CaptureProjection in the real loader layer

The regular `VK_LAYER_D4VR_trace` now instantiates `CaptureProjection` in an **opt-in diagnostic** submit/present cycle. An external diagnostic pose request updates the existing GPU camera buffers, selects original-render eyes, associates completed Capture leases and reads their actual image pixels through an owned GPU consumer. It does not replay application commands or call OpenXR.

This layer integration is verified with real X11 WSI and the Vulkan loader, using owned tagged geometry. **The new binary has not been run in Diablo.** The last actual game evidence remains `diablo-camera-updates-v1.json`. A connected headset is not required for this new diagnostic; it does not submit anything to SteamVR/Quest or provide continuous immersive game stereo.

## Explicit input and limits

Enable `D4VR_POSE_CAPTURE_DIAGNOSTIC=1` together with `D4VR_GPU_CAMERA_UPDATES=1`, the validated left/right packets and `D4VR_SEQUENTIAL_CAPTURE_DIR`. It uses the existing GPU Capture stream. Combining it with the dry run, XR binding probe or closed replay POC is refused. File-based `camera-pair.bin`, `eye.selection` and `present.request` do not compete for camera/eye ownership in this mode.

Atomically rename a regular file `pose.request` in the private directory. `core/diagnostic_pose.hpp` supplies a bounded classic-locale codec. Its fixed text version is `D4VR_DIAGNOSTIC_POSE_1`, followed by session/request/predicted-time/flags, both orientations/positions/FOVs, the affine game-view anchor and near/far planes. All fields are validated through PoseRequest; trailing fields, signed unsigned identities, invalid samples, overflow and files larger than 4096 bytes are refused. The source is **external diagnostic input**, not an observed XR runtime. A supplied predicted time is retained as supplied and is not a current display-time claim.

At most eight accepted requests are processed for a device. There is one active request, at most two selected leases, up to four deferred retirement entries and Capture's existing four images / 256 MiB. The diagnostic consumer owns one command buffer, one fence and one coherent readback buffer, bounded to 128 MiB including the driver's allocation size. It polls GPU fences instead of host-waiting in the normal present path. File I/O, CPU copying and private pixel export can still stall that path; this diagnostic is not a VR cadence implementation.

Writing `1\n` to a regular `pose.stop` file requests a terminal stop. No new sample is accepted. An unsubmitted pair is canceled and retired; an already submitted consumer keeps its leases until its fence completes. A new diagnostic session requires a controlled device/driver restart. The existing game-exit path is also safe: device work completes, the source/foreign leases are finished, resulting retirement markers complete, then private resources are destroyed.

The first eligible single swapchain presentation establishes the queue, family, extent and format. Resize or a different output contract is not dynamically recalibrated; a new controlled diagnostic session is required. Whole scene, dynamic effects, UI and common scene time remain unverified. The bounded present gap is 64 for this offline driver and is not a millisecond or headtracking-latency bound. There is no automatic deadline for a request that never produces an admissible left image; `pose.stop` or normal teardown cancels it.

## Submit/present/recording cycle

Before a tracked original submission, the layer offers the pending packet pair, runs the existing BoundaryUpdates dependency protection, and acknowledges only the updater's actual ordered packets. The snapshot taken before the successful original submission remains attached to the Capture generation. Selected-draw intervals and matching present IDs are retained at their original boundaries; late deliveries are not relabeled.

An atomic desired-eye value is published only from that protected cycle. `vkBeginCommandBuffer` snapshots it once for the command's entire recording. Both pipeline selection and presentation-blit capture use this **same command snapshot**. A different eye requested while another thread is recording does not change half of that command. Previously recorded commands are not rewritten; mixed, unknown or incompatible evidence remains inadmissible.

On present, completed leases are offered to CaptureProjection. Its record callback returns the real private images and explicit successive receipts, with sceneSequence zero. The isolated diagnostic consumer copies both images to its owned buffer and exports bytes only after its fence completes. The exact original sample, remapped GPU revision and each eye's capture/present/lease/packet observations are exported beside the private pixels. Reports always mark `actual_xr_pose_source`, `xr_submission`, `whole_scene_verified` and `common_scene_time_known` false.

All diagnostic state and queue submissions are serialized by the existing camera mutex. A lease delivered on a different queue is retained in the bounded retirement list. It is retired only during a later call that owns its actual producer queue, or during device teardown when the caller owns all queues. Transfer-only retirement markers do not invent camera readers. The GPU consumer itself uses the established same producer/consumer queue. No extra queue dependency is inserted between unrelated application submissions.

This driver makes **no XR calls while holding the layer mutex**. It intentionally does not introduce a concurrent runtime worker. Coordinating real runtime reentry, pacing and every borrowed-queue operation remains required before connecting ProjectionSession to this layer.

## Verification

`tools/pose_layer_validate.py` uses the actual loader/layer and X11 swapchain with two tagged triangles at depths two and four. It tests two diagnostic poses, short teardown, terminal stop and frames whose producer queue differs from the present queue, natively and inside Steam Runtime. All five enabled graphics queues are registered and original application draws/submissions are counted. Every exported eye's pixels must equal the application's independently saved original-render image at its retained presentation ID. Both eye disparity and the second pose's translation must vary with depth. Restoring original pipeline selection restores the original pixels.

The foreign-producer fixture explicitly serializes its own shared application resources with a timeline and waits for swapchain acquisition with a host fence. An earlier version of that fixture reported presentation-engine write races after the second camera update; semaphore waits alone in that test were insufficient for a clean validation result. The final proof covers the corrected fixture, **not** an asynchronous multiqueue game renderer or runtime coordinator. Its host waits are test scaffolding and not added to the layer's normal present path.

Portable tests round-trip all sample fields and exact 352 packet bytes, preserve a session ID larger than JavaScript's exact integer range, and refuse nine malformed inputs. Existing layer forwarding and disabled/legacy WSI capture checks also pass. The diagnostic readback buffer and repeated application-reference readback have explicit transfer/host-to-transfer reuse dependencies. Linux/Windows compilation and Linux ASan/UBSan cover the common components; they do not establish Windows D3D12/VR game support.

Final numeric evidence is `docs/proofs/layer-pose-capture-v1.json`. Game shaders, raw game images and private logs remain outside the repository.

## Next integration

The reusable GPU source already supports the deferred XR interface, but the layer driver currently supplies external diagnostic samples and a readback consumer. Replace that diagnostic driver with a coordinated real XR request/consumer cycle without holding non-reentrant layer locks over runtime calls. Preserve immutable original poses and the explicit successive/partial scope; do not promote them to strict simultaneous receipts.

Before a real game/HMD trial, validate the game-to-XR scale/anchor, optical FOV and clip/depth conventions. The earlier real Diablo translation proof preserved the game's projection, whereas PoseRequest derives an optical projection. This WSI fixture deliberately retains its own projection in its vertex shader and establishes view-transform feeding, pixel transport and ownership, not a calibrated Diablo projection. A first new game test must keep that distinction explicit. No physical SteamVR validation issue is resolved by this WSI result.
