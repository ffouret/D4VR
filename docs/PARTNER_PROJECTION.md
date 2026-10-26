# Partner projections: source routes and fixed-transform diagnostic

The real [camera block observation](CAMERA_BLOCK.md) found neighboring projections and inverse-related matrices. This step traces their original arithmetic routes and adds a narrowly scoped **fixed-transform experiment**, tested on owned GPU geometry. It has **not** run in Diablo and does not repair its wider-FOV holes. No game or headset is launched in this step.

## What the original shaders establish

`tools/camera_dataflow.py` consumes immutable, independently SPIR-V validated snapshots under the existing pinned Khronos grammar/write policy. Its closed SSA graph follows explicit numeric ID operands, composites, pure reviewed GLSL operations, pointer copies, and phi/select alternatives. Literal component indices and phi predecessor labels never become value edges. Dynamic pointer paths, local/private memory writes, unreviewed operations/effects, subroutines and unresolved interfaces are refused. Direct outputs and statically indexed output-struct members are resolved. Unit graph fixtures separately exercise literal/label ID collisions, cyclic phi traversal, aliases and refusals; those unit fixtures are not themselves shader-validation evidence.

The model combines vector components and all admitted alternatives conservatively. A reported route is a **possible data dependency**, not a proof that a branch runs, a coefficient is sensitive, or a fragment uses the result. Branch-condition sources are reported separately, without claiming attribution of control dependencies to each output.

The eight hash-verified original shaders observed in the preceding actual game session all pass this model and module validation:

| Source rows in camera binding 0 | Original SSA route |
| --- | --- |
| 80, 81 and 83, in six shaders | Output location 4. Its source model/view inputs use binding 1 rows 48–50, distinct from the Position path's rows 56–59. These camera rows do not reach a branch-condition value in those six graphs. |
| 42–44, in two shaders | Output locations 1 and 2, and branch-condition values. The analysis does not assign these control effects to individual outputs. |

The second projection is therefore not merely an unused UBO neighbor. Its source coordinates are kept distinct. These routes still **do not establish a current/previous-frame role, the fragment consumers, or responsibility for missing geometry**. In particular, changing an inverse solely because it is numerically related to another matrix is unjustified: it may cancel a source basis that our projection-only edit has left unchanged.

## Explicit experimental construction

`spirv_probe --stereo --helper-no-telemetry --projection-only-helper --projection-input --fixed-partner-projection` transforms both source windows 72–75 and 80–83 before original computations. The API adds `fixed_partner_projection=True`. The ordinary early-projection mode and default final-Position mode remain unchanged.

The new mode requires both original projection windows to be read, one original function, validated UBO extents and constant row/component paths. Each window is loaded independently and transformed with eight total calls to the existing validated helper. Eight separate Function-local output rows preserve the two results. The second input is not copied from the first: its source projection, source model/view coordinates and pass-depth coefficients stay distinct. Original inverse/model-view accesses and source stores are unchanged. No additional Position, Uniform or other original external store is added. A distinct `D4VR_FIXED_PARTNER_PROJECTION_1` marker identifies the construction.

This mode deliberately applies **one fixed diagnostic transform** to both projections. It contains no previous XR pose, history invalidation, jitter-history policy or temporal state. It must not be used as evidence of correct head-motion vectors or continuously updated VR. The current game bundle builder does not select this mode, and its validator explicitly states `game_bundle_authorized: false`. A prepared game experiment needs a separate explicit scope and admission checks after establishing the relevant consumers; no shader is silently deployed.

## GPU evidence and negative controls

The owned vertex fixture has a current projection, a second projection with different source jitter and far range, two distinct model/view inputs, and a source inverse contributing to a world-coordinate output. The fragment colors expose the inverse-coordinate, partner-XY and partner-depth paths. A separate uninstrumented render supplies optical projections and explicitly transforms both model/view inputs on the CPU. Its inverse cancels that changed reference basis; the instrumented render keeps its original model/view and inverse, placing the transform in the two projections instead.

Native and Steam Runtime executions each pass **48 independent references**: two source far ranges, three metric scales, matched/wider asymmetric optics, both eyes, translation and yaw. Each compares **12,582,912 RGBA bytes** with zero differing bytes, plus **605,086 D32 texels**, maximum depth error 0.000000178814. The enforced color boundary tolerance is at most 64 bytes per case; these executions use zero. Khronos synchronization validation loads in both environments and reports zero errors.

Three negative controls distinguish the dependencies:

- The ordinary early constructor changes only rows 72–75. It fails the same independent color reference when the partner output is left on the source projection, without Vulkan validation errors.
- Deliberately changing the inverse to the reference's new basis while leaving the instrumented source model/view unchanged changes 512 color bytes. The correct diagnostic preserves that inverse. This is an owned counterexample to updating all related matrices indiscriminately, not a measured semantic claim about Diablo's inverse.
- Replacing the second source projection with the first changes 21,336 color bytes. Its independent depth coefficients therefore matter to this reference; passing cannot be explained by substituting the primary projection for both inputs.

The original single-projection dependent-output fixture also passes its 48 references in both environments, with its expected late-Position failure and zero validation errors. Valid SPIR-V controls refuse a short partner UBO extent, a volatile partner load and a dynamic partner path; the graph also refuses the valid dynamic case. CI runs construction/route/refusal controls without a GPU. The Python suite contains 197 tests.

Six of the eight observed private original shaders additionally construct and pass SPIR-V validation in the new mode; 262 original store instructions remain, with only Function stores added. The other two have no admitted partner projection read and are explicitly refused. These are private construction artifacts, not a game bundle or an executed game render. No original game/vendor file is modified.

## Remaining work

Resolve the fragment consumers of location 4 and its source-coordinate/time relationship, then prepare a clearly labeled fixed-view game experiment with provenance and temporal limits intact. The two inverse-reading shaders also have control dependencies and are not candidates for blind replacement. Upstream culling, unselected stages/passes and screen-space history remain possible contributors.

The last actual geometric Diablo result remains [PROJECTION_INPUT.md](PROJECTION_INPUT.md): two successive partial views, wide-FOV holes, no common scene instant, no current XR pose and no Quest submission. An anchor/gravity calibration, a compatible physical SteamVR queue model, a continuous stereo cadence and native Windows D3D12/VR are still absent.

Aggregate evidence: [partner-projection-v1.json](proofs/partner-projection-v1.json). Private source identities, shader bytes, graph details and previous game blocks stay outside the repository.
