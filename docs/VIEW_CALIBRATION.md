# VIEW units, optical projection and source depth

The later [projection-input variant](PROJECTION_INPUT.md) applies the same
policy to original vertex projection reads, including dependent outputs. Its
new real-game trial still fails wide-FOV coverage; it does not supersede that
limitation or establish the missing scene dependencies.

The new opt-in projection policy combines optical X/Y with the original pass's scaled depth coefficients. It has now run in a stationary Diablo IV session through the regular layer's external-pose diagnostic. Six actual GPU pairs retain their own camera packets and presentation IDs. This is **partial, successive game rendering, without OpenXR or a Quest submission**. The widened-FOV capture has visible missing regions; complete game projection remains unresolved.

## Measured conventions and explicit choices

The existing private matrix capture contains 75 samples, including 24 distinct projections matching P00=1.784022 and P11=3.171595. Those values imply approximately 58.544° horizontal / 35° vertical FOV. The last row is `[0,0,-1,0]`. Two depth coefficient families, rounded in the capture, imply near≈0.3 and far≈80 / 200 **VIEW units**, under a finite, forward `[0,1]` interpretation. Jitter varies between samples. They are different pass ranges, not a single global clip plane.

The new actual game trial observes negative-height Vulkan viewports, including 2560×1440 and 1707×960, with minDepth=0 and maxDepth=1. This is bounded device-level observation, **not attribution of every viewport to a selected draw**. Positive diagnostic camera Y moves accepted terrain patches down; opposite camera X offsets move them in opposite horizontal directions. These measurements support the tested view basis and screen orientation. World gravity, the world-to-VIEW rotation and the physical tabletop anchor remain uncalibrated.

The trial chooses **0.1 meter per VIEW unit** and a 0.064-meter eye separation. This is an explicit artistic diorama size, not an inferred physical scale. The original view origin is the provisional anchor, with an identity basis and uniform scale. Choosing a comfortable physical height, placement and orientation needs further calibration. [OpenXR's coordinate conventions](https://registry.khronos.org/OpenXR/specs/1.1-khr/html/xrspec.html) use meters, +Y up and -Z forward; [Vulkan viewport rules](https://docs.vulkan.org/refpages/latest/refpages/source/VkViewport.html) explain the negative-height Y mapping.

## Why source depth is retained

Replacing a source projection with fixed optical near/far values changes normalized depth even for a pure lateral eye displacement. At a chosen metric scale this can disrupt depth comparisons between game passes. For a supported source perspective with coefficients A=P22, B=P23 and metric scale s, the new helper uses:

```
eyeView = inverse(eyePose) * gameViewInLocal * originalView
Xclip, Yclip = opticalProjection * eyeView  (X/Y components)
Zclip = A * eyeView.z + B * s * eyeView.w
Wclip = -eyeView.z
```

Each invocation uses its own original pass coefficients, including homogeneous W. The helper still removes the original X/Y jitter during unprojection. With uniform scale and pure lateral translation, normalized source depth is preserved. Rotation or forward movement changes depth as the new viewpoint requires; there is no claim that postprocessing, unselected passes or culling become correct automatically.

`ProjectionDepthPolicy::sourceScaled` requires a finite, proper uniform similarity: reflections, nonuniform scale, shear and nonaffine mappings are refused. Only the measured forward finite perspective family is admitted. Reversed, infinite or orthographic depth is unsupported. The GPU helper leaves a nonmatching source projection unchanged.

## Versioned input and compatibility

The std430 packet remains 176 bytes. Existing packet **version 1** and its flags retain their meaning. The new policy uses **version 2, flags 5**, with meters/VIEW-unit in `sourceGate.w`. Old helpers reject version 2 rather than silently using an incompatible depth mapping. `D4VR_STEREO_ABI_1` still identifies the unchanged binding/layout; it is not the packet's version field. A rebuilt private shader bundle is required.

`PoseSample.depthPolicy` survives mailbox revision remapping and is checked in successive receipts. The external diagnostic codec retains its exact version-1 format for ordinary optical requests. Version 2 uses `D4VR_DIAGNOSTIC_POSE_2` and appends policy `1` after the near/far line. The source-scaled policy uses those near/far values only to construct the otherwise unused optical Z row; the original pass supplies the actual GPU Z mapping.

The cooperative layer exposes the same policy through optional `D4VR_METRIC_SOURCE_DEPTH_SCALE`, with a uniform anchor scale. It remains a restricted diagnostic; it does not remove any queue, runtime-reentry, source-coherence or pacing constraints in [COOPERATIVE_LAYER_XR.md](COOPERATIVE_LAYER_XR.md). It was tested with the simulated runtime, **not physical SteamVR**. The game trial uses the separate `D4VR_POSE_CAPTURE_DIAGNOSTIC=1` input, no runtime calls and no competing camera file. `D4VR_VIEW_DIAGNOSTIC=1` records at most 32 unique viewport values and the features actually enabled in device creation.

## Reproduction and evidence

The default native build runs `view_calibration_validation`; Linux ASan/UBSan and Windows compile/test this portable math, policy and codec path. The GPU fixture accepts `--depth-calibration` with the original owned vertex, its projection-only instrumented version and the owned fragment shader. `camera_bundle.py --stereo --projection-only-helper --source-depth-calibration` requires both its existing version-1 GPU certificate and the new depth certificate before constructing a private bundle. No game bytecode or raw logs are published.

The 48 GPU cases exercise two source far ranges, three metric scales, both eyes, matching/asymmetric wider FOV, translation and yaw. They compare an instrumented render against an independent uninstrumented render supplied with transformed model/view and optical X/Y plus scaled source Z. Color comparison allows at most 64 boundary bytes per 262,144-byte image; D32 error must be below 0.000002. A negative control using fixed optical Z exposes approximately 0.499499 normalized depth error. Native and Steam Runtime synchronization validation pass. These are owned synthetic scenes, not game depth readbacks.

The version-2 WSI/loader diagnostic and cooperative mock-runtime trials check actual source pixels, lease retirement, cancellations, reentry and original pose identity. Their images are exact comparisons against the corresponding original application frames; they do not certify an arbitrary asynchronous engine/runtime. The portable strict-coherence validator is unchanged.

In the actual game trial, 26 of 45 contrasted terrain patches pass the predeclared MAE/uniqueness acceptance across all checked views. All 26 move oppositely for the eyes (left +34…44 pixels, right -44…-34), return to their original coordinates in both restores, and move down 10…14 pixels for a positive 0.01-meter camera Y offset. The 91.67°×91.67° diagnostic FOV produces visible missing/discontinuous regions. Culling, pass selection and screen-space dependencies have not been separated as causes. That capture is retained as a **coverage failure**, not a usable immersive view.

The actual device enabled synchronization2, dynamic rendering, timeline semaphores and buffer device address. Five graphics queues were registered; this trial needed no cross-queue camera wait. That does not establish SteamVR's dispatch/threading model. There is no device-destruction trace or final in-game fence accounting, and no Khronos validation in the game. The six requests are controlled samples with diagnostic times, not current XR poses. The two eyes remain separate presents; whole-scene and common-scene-time flags stay false.

Aggregate evidence: [synthetic and transport controls](proofs/view-calibration-v1.json), [actual Diablo trial](proofs/diablo-view-calibration-v1.json). The next steps are to resolve optical-FOV scene coverage, establish the tabletop anchor/orientation and verify the physical runtime against the restricted queue contract. Simultaneous scene production, continuous cadence and native Windows D3D12/VR integration remain open.
