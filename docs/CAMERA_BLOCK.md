# Lossless camera block observation

The previous wider-FOV Diablo trial still has missing regions after rewriting canonical projection reads. A new **observation-only** shader probe now reads the neighboring camera data from real Diablo, instead of guessing which other matrices should change. It finds concrete partner dependencies but does **not** repair rendering or establish their responsibility for the holes.

## Observer and bounds

`tools/camera_block.py` generates a helper for the existing pinned VKD3D diagnostic ring. `spirv_probe --camera-block-rows 113 --first-row 0` imports it as `D4VRBlockProbe_v1`. The constructor validates the canonical fixed float4-array Uniform layout and the complete requested extent. This mode is separate from geometry, stereo and early-projection transformation; those defaults remain unchanged.

Only original vertex index zero loads the neighborhood, at a normal return from the original entry point. The helper additionally selects the previously measured projection scale numerically. This gate identifies a perspective family, not a pass's semantic role. One sample is emitted per 4,096 qualified invocations, at most 64 samples. An invocation/sample index is **not** a frame or presentation ID. The budget bounds ring writes; observer calls and eligible UBO reads can continue until the application exits.

Each record contains 113 float4 rows (1,808 source bytes). Raw float bits are logged as hexadecimal uint words using the existing ring protocol, retaining signed zero, subnormals, infinities and NaN payloads. The 64-record source payload is bounded to 115,712 bytes; message headers add ring traffic. The isolated game launch uses a 16 MiB ring. No projection, Position, original UBO or original external resource is written; only Function locals and the private diagnostic ring/control counters are added. Source store instructions are retained, not replayed. Reading the full declared UBO still costs GPU work, and runtime buffer validity/resource effects remain application-dependent.

The parser rejects incomplete, duplicate, decimal-payload and out-of-budget records. Separate ring starts keep records separate. It accepts no more than 64 MiB of log input or 1,024 candidate groups. `camera_block_report.py` searches finite four-row windows without assuming four-row alignment. Both matrix products must approximate identity before reporting an inverse; ambiguous matches stay explicit. Its conservative static instruction inventory retains direct Uniform pointer copies and reports dynamic loads and unreviewed pointer producers. It is not an output dataflow, branch execution or ordering proof.

## Owned GPU controls

`tools/camera_block_validate.py` builds and SPIR-V validates the helper, original and observed owned shader, and refuses a valid shader whose UBO is one row too short. CI performs these construction checks. Optional GPU arguments additionally test exact source bits, sparse sampling, the last permitted sample, ring wrap, the exhausted budget and the nonmatching perspective gate.

The native and Steam Runtime controls each compare **5,424 source UBO bytes** and **1,572,864 rendered RGBA bytes** exactly. Special float values occupy otherwise unused rows. Both executions load Khronos synchronization validation and report zero errors. These are owned synthetic resources; exact pixel equality is not claimed for Diablo. The private bundle constructor requires both GPU environments, matching source/helper hashes and the refusal control before preparing game overrides. This local certificate is a reproducibility gate, not a signed GPU attestation.

## Actual Diablo observation

The new trial uses 18 previously observed, hash-verified original vertex shaders, all rebuilt and SPIR-V validated with this read-only helper. It does **not** load the native trace layer or use the paired-eye camera transform. The selected character enters normally, remains stationary, and exits through the game UI after sampling. No XR runtime or headset is used.

The log yields **64 complete records from eight shader identities**, with no incomplete/duplicate groups or rejected rows. Every record has the same numerical relationships, using zero-based float4 row indices:

| Relationship | Observed result |
| --- | --- |
| Rows 34–37 and 38–41 versus 42–45 and 91–94 | Each of the four cross-pairs satisfies both inverse products in all 64 records; maximum absolute identity residual 0.000016466. |
| Perspective windows beginning at rows 72, 76 and 80 | All 64 contain all three. The two alternatives differ from row 72 only at flat matrix components 2 and 6 (row 0/1, column 2). |
| Direct inverse of the row-72 projection | No separate finite four-row window meets the 0.00002 two-sided tolerance. This is not an absence proof for other resources or encodings. |

The static inventory of the eight observed original shaders resolves all their direct reads: all eight read rows 72–75, six also read rows 80–83, and two read rows 42–45. The existing early-projection construction redirects only rows 72–75. Thus specific source inputs remain unchanged by that construction. **Whether those inputs influence the problematic pixels, represent previous-frame transforms, or require another update is unproven.** Their mathematical similarity alone cannot establish temporal roles. The inverse pairs also do not calibrate world axes, gravity or the tabletop anchor.

The original game image looks normal in the captured world view; no exact before/after game pixel comparison or game validation-error count is claimed. The launcher exits with code zero, all owned processes exit, and the original game/vendor audit and 262 named exports match before and after. Raw blocks, shaders, images and logs remain private outside the repository.

## Next dependency to establish

Trace the observed row-80 projection and row-42 inverse reads into their output calculations and partner resources, including any temporal state, before choosing a narrowly scoped transform. Use an owned dependent-output control for any resulting rewrite, followed by a new prepared game comparison. This probe supplies real data for that work; it neither removes upstream CPU/GPU culling nor covers other stages/passes or screen-space processing.

The last actual geometric stereo result remains the [early-projection trial](PROJECTION_INPUT.md), whose wide-FOV holes persist. Current views are successive and partial, with no shared scene time or current XR pose proof. Physical SteamVR queue/dispatch synchronization, the tabletop anchor and native Windows D3D12/VR remain unresolved. **No Diablo image has reached the Quest and no continuous immersive cadence is established.**

Aggregate synthetic controls and actual observations: [camera-block-v1.json](proofs/camera-block-v1.json).
