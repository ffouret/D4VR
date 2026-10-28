# Early projection loads: dependent vertex outputs

An opt-in construction variant now transforms the projection **before the original vertex computations**. This keeps admitted projection-dependent varyings and Position calculations on the same requested view. A new owned GPU control demonstrates a failure that the older final-Position edit cannot solve. The variant has also run in real Diablo IV: stereo terrain displacement and restoration remain measurable, but **the wider-FOV holes persist**. This does not establish the cause of those holes or repair immersive coverage.

## The distinction

An original vertex shader can compute both Position and other fragment inputs from its projection:

```glsl
vec4 clip = projection * viewPosition;
gl_Position = clip;
projectedVarying = clip.xy / clip.w;
```

Editing Position only at return leaves `projectedVarying` on the old view. In the new test fragment shader, a projected-coordinate visibility test then discards valid geometry. The early variant makes the original projection reads return `targetProjection * eyeFromGameView`, so both original calculations agree with rasterization. Independent uninstrumented camera/model references pass; the old late variant fails the same control. This is a concrete owned test, not proof that Diablo's fragment shaders implement that visibility test.

The existing telemetry-free helper is reused without adding memory permissions. Applying it to each of the four columns of the **original** projection yields the effective projection columns; the constructor transposes them into four invocation-local row variables. Every admitted original load of those rows then reads the local row. There is one initialization before the original entry block's executable work, four helper calls per vertex, and no extra late Position store. Original output and other source store instructions are retained. No original UBO or game/vendor file is written. Source-scale depth still uses the original pass coefficients, as in [VIEW_CALIBRATION.md](VIEW_CALIBRATION.md).

## Closed admission and resource limits

Use `spirv_probe.py --stereo --helper-no-telemetry --projection-only-helper --projection-input`. The default construction remains the final-Position mode. Early construction requires one original vertex function, the already-validated fixed float4-array UBO layout and statically resolved paths to its projection rows. Whole-row loads and constant component loads are supported. Uniform pointer copies retain their canonical paths; unsupported pointer producers, dynamic camera row/component loads, additional functions, volatile/decorated loads and unavailable shapes are refused. Unknown inputs never become a guessed projection.

Only Function storage is added; there are no new image allocations, submissions, queues or leases. The current construction executes four helper calls per invocation and may cost more GPU work; no VR cadence is certified. It loads source projection rows before the original body, so source resource aliasing/ordering still requires audit. Preserving source store instructions does not certify their effects or authorize shader replay.

`camera_bundle.py --projection-input` additionally requires source-depth projection-only stereo and `--projection-input-certificate`. The certificate must match the current constructor/fixture source hashes and helper and contain both native and Steam Runtime dependent-fragment GPU controls, plus the expected failure of late construction and conservative refusal controls. A static-only certificate cannot authorize a game bundle. This is a local reproducibility gate, not a cryptographic attestation of GPU execution.

The private corpus admits **5,373 of 5,378** previous candidates. Five multi-function modules are deliberately excluded, with their original game shaders left in use. All admitted modules are rebuilt from hash-verified originals, linked and SPIR-V validated. None of the excluded shaders is silently converted with the old mode. This is still partial game coverage.

## New evidence

`tools/projection_input_validate.py` compiles the owned dependent-output shaders and validates four refusals against valid SPIR-V: dynamic row, dynamic component, an additional original function and a volatile projection load. A valid Uniform pointer copy is admitted. Without GPU arguments the tool performs only construction checks, which are included in CI. With `--gpu-fixture`, `--validation-layer` and optionally `--runtime`, it runs the GPU references and negative control.

The 48 dependent-output cases pass natively and in Steam Runtime, with two source far ranges, three scales, both eyes, matching/wider asymmetric optics, translation and yaw. D32 reference errors remain below 0.000002; color comparison retains the existing 64-boundary-byte tolerance. Khronos synchronization validation reports zero errors. The late-Position variant fails the color/reference comparison as expected, without a Vulkan validation error. The original simple-color fixture's 11 version-1 references and 48 depth references also pass with early construction. These scenes are synthetic, with owned resources.

The new actual Diablo trial retains four successive pairs: reference, wide FOV, stereo and restore. It accepts 22 of 40 contrasted local terrain patches. All 22 move oppositely for the eyes and return to the original coordinates. The 91.67°×91.67° image still has visible missing/discontinuous regions. It was sampled in different lighting from the previous trial, so a pixel-level improvement comparison between those sessions is not claimed. There is no game D32 readback, whole-scene proof, common instant between eyes, current XR pose or Quest submission. The existing layer binary, GPU pool and queue protocol were reused unchanged.

An offline check of the preceding wide capture additionally finds 16 accepted terrain correspondences after undoing the exact requested optical scale; all favor that projection over an unchanged-image hypothesis. This supports the measured projection mapping, not scene completeness. A static audit finds ClipDistance output stores in 329 original candidates and no direct SSA alias from Position stores to location varyings in the tested corpus. Those counts do not identify the cause of the holes; clip planes are not disabled. Original ClipDistance expressions remain in the early variant and see the new rows only if they read those admitted projection rows.

**Upstream CPU/GPU culling, other shaders/passes, inverse or partner camera matrices and screen-space dependencies are unchanged.** Geometry omitted by an earlier stage cannot be recovered merely by rewriting a vertex projection. The next diagnostic must separate those dependencies rather than repeat this trial or present the wide image as usable VR. The tabletop anchor/gravity and physical SteamVR queue/dispatch contract remain unverified; native Windows D3D12/VR integration remains absent.

Aggregate controls and actual game result: [projection-input-v1.json](proofs/projection-input-v1.json). Private shaders, pixels and raw logs remain outside the repository.
