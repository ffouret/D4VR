# Capture-owned CPU state at original writer draws

`D4VR_SAMPLED_WRITER_STATE=1` adds a bounded CPU observation to each retained
draw in a capture-owned source prefix. It saves the original draw arguments,
the first 32 fragment push-constant bytes and their writers, and conventional
viewport/scissor declarations. The values belong to that original command
recording and draw ordinal. They do not identify a sampled image or establish
pixel coverage.

This extends the [capture-owned writer prefix](CAPTURED_WRITERS.md). The
[owned evidence](proofs/sampled-writer-state-v1.json) is separate from every game
trial and from the earlier [sampled-image pixel control](SAMPLED_WRITER_CONTROL.md).

## Configuration and unchanged forwarding

The state option requires the existing `D4VR_DEPTH_SOURCE_HISTORY=1` and
`D4VR_SOURCE_CONSUMER_HISTORY=1` observer. Without consumer observation it is
inert. The ordinary capture and camera diagnostic retain their own independent
configuration and eligibility rules.

The new observer creates no GPU resources, commands, fences, waits or
submissions. It never changes an application's arguments, stage masks,
descriptors, pipelines or returned errors. The combined color/depth laboratory
still has its existing GPU producer and consumer operations; the CPU-only claim
applies to this added state observer.

With the option disabled, the new setters do not advance SourceHistory ordinals
or add state fields. The shader-object invalidation hook is also inert in that
mode. CPU forwarding controls compare the original pointers, pNext, ranges,
counts, bytes and distinct downstream alias calls with no observer, with the
observer disabled, enabled and faulted.

## Push values, layout lifetimes and compatibility

Only the intersection of an original push update with bytes `[0,32)` is copied.
The fragment bank is separate from other shader stages. Partial updates retain
untouched words. Each of the eight words keeps its known mask, original setter
ordinal, stage flags, offset, size and immutable layout declaration. Unknown
words serialize zero bytes; unknown decoded indices serialize null.

Layout declarations have unique observer generations, diagnostic raw handles,
flags and bounded push ranges. Destruction removes the current handle mapping,
while an earlier pipeline or recording may keep its immutable lifetime. A new
lifetime at the same handle cannot replace that snapshot. Original pipeline
declarations are captured before the driver call and published only for actual
non-null outputs. Original application metadata remains distinct from the
private pipeline selected by other diagnostics.

Binding another pipeline does not erase the saved push bytes, following the
[Khronos push-constant lifetime rule](https://docs.vulkan.org/guide/latest/push_constants.html#_binding_non_compatible_pipelines).
At a draw, the
observer rechecks compatibility against the bound pipeline's frozen layout.
Its sufficient rule requires complete, identical ordered push-range tuples;
otherwise compatibility stays unknown. Both source and bound layout signatures
are carried in the draw observation, so the independent auditor recomputes this
result without consulting a live registry. Different handles may be compatible.

Words two and six are decoded as native uint32 values only when their bytes and
layout compatibility are known. Those integers are merely candidate shader
indices. They do not prove descriptor placement, the selected image/view/sampler,
UBO values or a shader read. No mapped-memory or general resource tracker is
enabled.

## Viewport, scissor and direct-draw arguments

Static, regular dynamic, WithCount and unknown modes remain distinct. The
observation retains index zero, counts, setter/count ordinals and pipeline
generations. A call affecting index one does not become evidence for index zero.
A regular setter does not establish a WithCount count. Static pipeline binds
invalidate the dynamic banks conservatively, which can leave valid hybrid state
unknown rather than reuse a stale value.
The underlying static/dynamic lifetime is described in the
[Khronos dynamic-state guide](https://docs.vulkan.org/guide/latest/dynamic_state.html#_dynamic_state_lifetime).

Viewport values are six float32 bit patterns, preserving negative height,
signed zero and nonfinite values. Scissor offsets remain signed integers and
extents unsigned. Their known conventional declarations do not establish the
complete raster state, clipping, conditional execution, effective color writes
or coverage. A static GPL declaration keeps its actual pre-raster owner
generation; ignored link-only pointers are not interpreted as state.

The layer observes classic push commands, PushConstants2 core/KHR spellings and
viewport/scissor WithCount core/EXT spellings through their own original aliases.
Unhandled extensions, pipeline flags, incomplete GPL ownership and unsupported
state remain explicit unknowns. Secondary execution, generated commands and
shader-object binding invalidate relevant state. Direct and indexed draw
arguments are copied from their actual CPU calls; indirect arguments are not
invented from GPU buffers.

## Freeze and bounds

The draw copies its state before later setters can change the recording. The
existing prefix then serializes those values before the exact original submit,
with its original source lifetime, recording, exclusive BLIT cutoff, submission
and capture sequence. Export never reconstructs state from a current command,
layout or camera. CPU AFTER results and the producer's GPU fence remain separate.
Reset, reuse, late completion and repeated submissions cannot retag an old
receipt as a new unique occurrence.

The layout catalog admits at most 8,192 simultaneously live immutable snapshots,
including destroyed layouts still retained elsewhere. A layout keeps at most
32 ranges and eight pNext type labels. The pipeline registry remains bounded by
the existing 65,536-entry limit; recording and event limits remain 32,768 and
8,192. Setters retain fixed current state rather than an unbounded history.

Per-draw JSON has a 16 KiB bound. The complete capture-owned prefix retains its
existing four-payload, 64-event, four-occurrence and 128 KiB-per-payload bounds.
Serialization exhaustion produces explicit unavailable evidence and does not
reject an application call or enlarge those limits. The serialized payload pins
no live recording, event or layout lookup.

On the tested native ABI, state payloads are 616 bytes per recording, 656 per
saved draw and 456 per layout. Multiplying by their separate caps gives
20,185,088, 5,373,952 and 3,735,552 bytes respectively. These estimates exclude
allocators, registries, shared/weak control blocks, pipeline metadata and other
history fields. They are not a total memory cap.

## Owned controls

The WSI fixture executes the same original calls and enabled device features in
observer-off and observer-on controls. It records two original draws per frame:
one static pipeline and one regular dynamic or WithCount pipeline. A separate
compatible layout supplies the real 32-byte vertex Item through a common
VERTEX|FRAGMENT push range, followed by two partial writes. The owned fragment
shader does **not** read these push constants. These controls verify CPU bytes
and state retention, not a sampled resource index used by that shader.

Features and extensions are queried and explicitly enabled before optional
WithCount/PushConstants2KHR calls. The fixture saves 128 independent original
draw controls per run. Four native/Steam Runtime runs therefore save 512
controls, but only eight observer-on delivered prefixes contain the sixteen
draw states actually compared. Those sixteen comprise eight static, four
regular dynamic and four WithCount states. Selected receipts cover classic
push, PushConstants2KHR and core count setters. EXT count aliases occur in the
original control logs and separate CPU forwarding tests, but are not selected
in these physical image receipts.

Off/on original color and geometric-depth references compare 67,108,864 bytes
exactly across the two environments. All sixteen delivered color images compare
4,194,304 bytes to their original same-process source frames; the eight
observer-on images account for 2,097,152 bytes. Khronos synchronization validation
reports zero errors. Existing submission/draw counts, saved BLIT stages,
producer completion, lease retirement and diagnostic shutdown are checked too.

The independent read-only auditor rejects 22 in-memory alterations of actual
positive receipts, including changed raw bytes, masks, layout signatures,
generations, draw/setter ordinals, foreign viewport state, source identities,
CPU AFTER results, completion stages and unavailable resource/coverage claims.
Original files remain unchanged. Twelve new auditor unit tests and the full
336-test Python suite pass. The pure model and SourceHistory integration add ten
and sixteen CPU cases respectively, including 80 discarded recordings, handle
reuse, out-of-order completion and quota exhaustion. Common Windows objects
compile; these results do not establish a native Windows D3D12/VR integration.

## Remaining work

A [separate completed Diablo trial](proofs/diablo-sampled-writer-state-v1.json)
retains twelve image receipts and eleven matched original three-vertex draws.
Their 28 raw fragment push bytes survive delivery, but the pipeline's unmodeled
fragment-shading-rate-attachment flag leaves its bound layout and conventional
viewport/scissor declarations unknown under the current conservative policy.
The saved raw words are not certified resource indices. This trial neither
identifies the sampled image nor attributes the persistent rectangle to raster
state. Any finer handling must establish which declarations remain trustworthy
without claiming effective fragment-shading-rate or complete pixel coverage.

The earlier eighteen-readback sampled-image experiment uses privately verified
unchanged stage modules with owned textures and an independent pixel oracle.
It is a distinct proof and was not rerun for this observer. The present WSI
fixture uses repository-owned shaders and does not sample its observed push
indices.

Actual resource selection still needs independently established descriptor
placement and lifetimes; UV offsets need an identified UBO and a safe finite
observation window. Neither state declarations nor render-area dimensions prove
which cause explains a game's rectangle or missing geometry. This proof contains
no game rendering, coverage repair, physical SteamVR certification, XR submission
or immersive continuous VR result.
