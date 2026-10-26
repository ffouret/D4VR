# Post-projection depth suffixes

The same-session source history identified four original vertex contents without
a private variant. Two have POSITION inputs and the canonical projection, but
change clip Z after it. The existing general selector excludes these final
expressions. This step recognizes the two closed forms, preserves the original
suffix, and tests projection input replacement with the correct clip units.
It does not expand the selector or install a game bundle.

## Closed original contract

`tools/projection_depth_suffix.py` validates one immutable SPIR-V snapshot with
the pinned grammar, write policy and `spirv-val`. It accepts a single entry
function, the canonical float4 camera UBO at set 3/binding 0, fixed rows 72–75,
and a common source vector. Projection multiplication must use four dots or the
reviewed ordered multiply/Fma form. The final position must have one of these
exact suffixes, with X, Y and W preserved:

- `Z = NMin(projected_Z, projected_W)`;
- `Z = projected_Z + (0.1 * row51.x + 0.33) / projected_W`.

The bias form requires the original separate multiply and add, with exact
float32 constants. It does not substitute a fused affine operation. Calls,
alternative control flow, dynamic or volatile accesses,
unmodeled memory and extra Position stores are refused. Other outputs and
resources must pass the existing closed route/write policy. These are static
expression and effect checks, not claims about execution, visibility or pass
roles. The two other unmarked contents remain outside this contract.

## Why clip magnitude matters

Let the original projected position be `q` and a uniform scale be `s > 0`.
The metric helper can produce `s*q`, which has the same normalized position as
`q`. An original bias `k/W` is not homogeneous: keeping that suffix after `s*q`
changes normalized depth to `Z/W + k/(s*s*W*W)`.

The opt-in `D4VR_SOURCE_CLIP_UNITS=1` helper divides its complete clip result by
the source-depth scale inside the matched source-depth branch, before the
original shader runs the suffix. The independent reference uses the original
shader with transformed source coordinates and the target projection divided
by `s`. This covers translation, rotation and asymmetric fields of view, as
well as pure scale. The clamp is homogeneous under positive scale; it provides
an additional preservation check rather than the reciprocal-W witness.

The default macro is zero. Its compiled telemetry-free helper stays byte
identical to the previously validated helper. The packet remains 176 bytes;
source-depth mode remains version 2, flags 5. Inert/unsupported packet branches
and the version-1 optical behavior remain covered by separate controls.
Projection-column helper calls can legitimately have W=0. The original bias
singularity for a final position with W near zero is neither removed nor
certified by these tests.

## Owned GPU evidence

[Aggregate proof](proofs/post-projection-depth-v1.json). Ten physical GPU
processes run the same owned scene in native and Steam Runtime environments:
corrected bias, corrected clamp, naive bias, late bias and late clamp in each.
All use Khronos synchronization validation with zero reported errors.

The four positive executions cover 144 reference configurations and 1,261,570
common D32 texels. Maximum absolute D32 error is 2.38419e-7, below the 2e-6
limit. Coverage has a 32-pixel contour allowance and color allows one LSB per
channel; the observed comparison has no coverage or color failures. This is
not a claim that every positive RGBA byte is identical. Twenty inert cases
compare RGBA/D32 exactly; four version-1 cases use independent optical references.
The clamp is active in twelve cases, with 41,576 covered reference texels at
depth 1. Finite owned final-position W ranges from 1.23125 to 229.762.

The naive helper has eight depth-only bias witnesses across both environments,
with maximum error against the correct reference of 0.102773. Its 72 independent
`k/s²` predictions match within 2.98023e-7. Late Position rewriting also fails
the owned projected-varying references. That extra varying belongs to the test
scene; it is not attributed to either original game shader. The owned clamp
fixture changes one finite-input FMin to NMin and revalidates it; this does not
claim general NaN equivalence or change the original game module.

## Local candidates and limits

`tools/post_depth_candidate.py` constructs one explicit private candidate from
frozen original/helper bytes. It requires both the owned GPU certificate and
the standalone helper certificate, bound to current sources and helper hashes.
Missing environments or negative controls, static-only proofs, invalid metrics
and source mismatches are refused. Before linking, original stores and suffix
arithmetic keep their exact instructions and operands. After linking, the full
write policy is checked and original Output-store counts are preserved.
Private snapshots and results use mode 0700/0600 on the tested POSIX host.
These mode bits do not establish a Windows ACL policy.

Two original modules have been recognized, constructed and SPIR-V validated.
Only Function stores are added; the original suffixes remain. Their results
explicitly retain `game_bundle_authorized=false`, `game_executed=false`,
`xr_submission=false` and `coverage_repaired=false`. They are not selected by
the general bundle builder or installed in Diablo.

The owned-controls step above launched no game or headset. A subsequent targeted
game experiment is described below. Initialization, complete coverage, common
scene time, temporal history and the physical SteamVR queue contract remain
unproved. A matching expression alone does not establish missing-pixel causality.

## Explicit admission and actual Diablo experiment

`tools/post_depth_admission.py` creates a separate private diagnostic bundle:
the 5,377 baseline entries remain byte-identical and exactly two frozen candidates
are added. The policy is `fixed_monoscopic_no_xr`. It checks pinned original
identities, mandatory SHA256 values, both certificates and current source hashes,
closed suffix/effect contracts, and a deterministic reconstruction of each
candidate from its original/helper snapshots. The read-only launch gate checks
the evidence chain, all 5,379 entries and the pinned runtime again.

The new bundle explicitly has `gpu_fixture_passed=false` and
`combined_bundle_gpu_validated=false`. Evidence for the unchanged baseline and
the owned suffix controls is kept separately. The generic bundle validator
refuses this bundle; the private launcher uses the distinct admission gate.
Thirteen CPU orchestration/tampering controls mock compiler operations and forbid
actual subprocesses. They include an uninstrumented original retagged as a
candidate, absent hashes, altered snapshots, source/certificate mismatches and
broken baseline/target links. These units are not SPIR-V or GPU evidence.

[Actual game proof](proofs/diablo-post-depth-v1.json): one new Diablo session,
without a headset or OpenXR, produced three D24 readouts and six successive
monoscopic color pairs at reference/wide/return mappings. The script took
18.204 host seconds. Holds span 5,414 / 5,398 / 5,429 host milliseconds; depth
retains the warm-pair packet and color has a later, distinct revision.
Normal UI exit completed, the launcher returned zero, all owned processes exited,
and checked originals plus 262 exports match before and after.
The bounded producer-status reports all precede the readouts, so no final
failure/fence/lease counters are claimed. Three complete export events and their
matching raw readouts are observed; the game was not relaunched for final counters.

The same-session joins pass 253 CPU checks. Each retained source prefix has
289 draw commands: 270 with a selected private variant and 19 without. The
targeted bias/clamp contents account for 38/10 substituted draws per prefix,
114/30 across three disjoint prefixes. Content joins use live FNV1a64 plus size
against SHA256-verified local candidates; this is not a new cryptographic hash
of a live module. Pipeline metadata describes application stages before private
specialization. Selection does not establish a helper branch's actual execution
or the contribution of a draw to the final image. Prefixes remain incomplete,
with 38 aggregate overflows and unknown effective write state.

The wide image still has large cutouts. Of 140 diagnostic patch candidates,
136 pass optical-depth/return thresholds, 29 pass color/return thresholds and
27 pass both. A predefined central region inside the original-view footprint
has median raw-D24 difference 0.494 and p95 39.34 against the inverse-optically
mapped reference, while its luma MAE is 10.17. Depth is raw and nonlinear;
this is a useful local correspondence, not a proof of initialized or complete
terrain under missing color. Saturated D24 words number 334,333 at wide view and
zero at reference/return; saturation alone is not a culling or initialization
proof. The older baseline session has different time/lighting and is explicitly
not a pixel-exact causal reference for these new candidates.
Color and depth have different acceptance criteria; their counts do not directly
measure a difference in scene coverage.

The next targeted question is how the observed depth and its destination are
consumed by color/screen passes, particularly around the central rectangular
color boundary. Outer coverage and upstream visibility remain separate open
questions. No Diablo image has been sent to Quest and this remains a partial,
successive diagnostic rather than immersive continuous VR.

## Reproduction

Run `python -m unittest discover -s tests -v` for the portable CPU checks.
`tools/stereo_helper_validate.py` builds all three helper modes.
`tools/post_depth_validate.py --help` documents static construction and optional
physical GPU execution. Without a GPU fixture it produces construction evidence
only and never authorizes a game bundle. CI compiles the owned shader forms and
checks their closed contracts; physical GPU evidence is separate.
