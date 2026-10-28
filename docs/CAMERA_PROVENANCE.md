# Camera packet evidence attached to produced images

`Snapshot` and `GpuFrame` now carry a value-owned `FrameCamera`. It contains two separate observations:

* **copySubmit**: the validated camera packet snapshot taken before the original successful submission containing the presentation copy. The layer captures mutable packets under the same device submission mutex as `BoundaryUpdates::before`, the original submit and the completion marker. The packet's GPU write/read dependencies are already queued before the original vertex readers. A completed producer fence remains required before delivering a GPU lease.
* **selectedDrawInterval**: the packet shared by the tracked selected draw submissions between the observed presentation calls. It is stored at the first matching present, even if the producer fence is still pending and delivery happens at a later present.

Each observation has a status (`unknown`, `uniform` or `mixed`), first/last successful-submission observation IDs, and an exact 176-byte `EyeState` only when uniform. IDs identify CPU observations of successful submissions, not a total execution order between GPU queues. The layer's JSON export contains the packet eye, revision and exact ABI bytes as hex. The API owns the actual structured packet by value, not a pointer to a mutable request or mapped GPU buffer.

`CameraPair` validates both packet ABIs, eye identities and the shared sequence before they can be sampled. The submission path snapshots immutable packets too. A mutable copy on a queue outside the protected graphics family receives unknown camera evidence. Failed original submissions are not stamped. This changes neither the application's submit arguments nor its synchronization objects.

## Conservative interval matching

`CameraInterval` records only successful original submissions and the selected draw counts/masks already tracked by the layer. Two different packet sequences or matrices in one eye's interval produce `mixed`. An unknown selected submission or unclassified command buffer leaves `unknown`; later uniform draws do not repair missing history. An interval containing both selected eyes is mixed. Empty submissions with no unknown commands do not contribute selected draw evidence.

`sameDiagnosticPacket()` is true only when the copy and selected draw interval are both uniform and contain exactly the same packet bytes. The scopes stay distinct: a uniform copy made under camera B does not relabel selected draws recorded as camera A. The caller supplies a validated pre-submit packet snapshot; `Capture` cannot independently infer shader reads from arbitrary Vulkan commands.

A frame's evidence is frozen at its original submit/matching present. Delivery at a later present does not overwrite it. Resetting the original command buffer, mutating/destroying an input snapshot, receiving a new camera request and reusing an image slot for another lease cannot change an already delivered value. Reused slots clear both observations; missing metadata stays unknown instead of inheriting the old generation's packet.

## What this does not establish

These are **diagnostic camera packet observations**, not an OpenXR pose receipt. The selected draw interval is not the resource/dependency closure of a complete game frame. Some passes may reuse older resources, be outside the shader gate, or be unclassified. Capturing the final image under a packet does not prove that every pixel used that packet. Both views still come from different presentations. There is no common scene timestamp, actual XR pose source, game-to-OpenXR bridge or continuous immersive Diablo rendering here.

The JSON explicitly keeps `whole_scene_verified`, `xr_pose_known` and `common_scene_time_known` false, even when both observations match. The OpenXR presenter's strict same-scene/current-pose receipts are unchanged. A later approximate temporal prototype must declare and verify its own mode instead of converting this evidence into strict XR receipts.

## Verification

`camera_provenance_validation` is a driver-free test built by the default native CMake configuration on Linux and Windows, and included in the Linux ASan/UBSan checks. It covers value ownership, malformed pairs, same-sequence/different-matrix conflicts, mixed revisions, missing commands/camera evidence and refusal to infer another eye or repair an unknown interval.

`presentation_gpu_lease_ownership` additionally holds the producer fence, changes the input packet before completion, and supplies a newer interval at delayed delivery. It verifies the old packet/present/submission remain unchanged, and a reused slot with no packet has unknown evidence.

The Vulkan GPU fixture verifies 37 camera packet copies per execution alongside exact owned image pixels. A real event keeps the first producer pending while its caller switches to another packet/interval. Real X11 WSI tests exercise the complete layer with immutable and GPU-updated camera packets, CPU snapshots and GPU leases, native and Steam Runtime. They compare every emitted packet's ABI bytes, eye, revision and submission ID against the camera actually queued by the controlled fixture. Three camera updates, the application's draw/submission counts and restoration of original pixels are checked too. These are owned synthetic scenes; no game or headset is launched.

Reproduce with `tools/camera_provenance_validate.py --help`. The aggregate is `docs/proofs/camera-provenance-v1.json`. Previous actual Diablo camera translation evidence remains separately pinned to its tested source in `docs/proofs/diablo-camera-updates-v1.json`.
