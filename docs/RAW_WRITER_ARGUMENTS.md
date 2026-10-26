# Raw writer setter arguments

The opt-in sampled-writer observer retains original viewport/scissor setter
arguments independently of whether it can interpret the bound pipeline. A saved
argument is a CPU call fact. It does not establish a current operand, execution,
clipping, raster coverage, a sampled resource or a complete image.

Earlier receipts omitted the setter payload when pipeline interpretation failed.
The new schema retains an independent bounded argument snapshot alongside the
existing interpreted operands. Historical schema 1/2 files remain unchanged;
readers never reconstruct omitted arguments from a later pipeline or recording.

The raw bank retains one index-zero candidate per viewport/scissor, rather than
an array or unbounded call history. Only the already admitted safe single-value
case is read. Candidate origin, form, first/count and capture epoch remain
separate from the most recent observed call. WithCount has no first argument;
its raw `first` is null. Calls for another first index do not relabel an older
index-zero value as their payload. Unsupported/null/multiple-value attempts are
explicitly unavailable. Float values retain their exact bits, including signed
zero and NaN payloads, independently of conventional validity.

A static/unknown bind or command invalidation can invalidate interpretation while
leaving the original historical call fact intact. Epoch comparisons make this
visible. Reset or a new recording clears the bank; already captured draws and
leases preserve their original values. No draw is retagged with a later setter,
current pipeline or current camera. Raw arguments never clear an unknown reason
or make an interpreted operand effective.

The narrowly reviewed raster subchain is
`VkPipelineRasterizationDepthClipStateCreateInfoEXT`. Under the actually consumed
pre-raster owner, a unique node with reserved flags zero no longer prevents
independent conventional viewport/scissor declarations. Its type and reserved
flags remain visible. The observer does not read or modify `depthClipEnable`;
the clipping effect remains unknown. Other unknown siblings, duplicate nodes,
truncation, unsupported modes and ambiguous ownership retain their refusals.
Ignored GPL structures are not dereferenced.

The structure controls Z clipping and reserves its flags at zero according to
the [Khronos structure contract](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineRasterizationDepthClipStateCreateInfoEXT.html).
This independence is not permission to change the game's depth-clip behavior.
Any owned GPU control using the structure must explicitly query and enable the
[required device feature](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceDepthClipEnableFeaturesEXT.html).
An unsupported configuration cannot be counted as a passing execution.

Original application dispatch, arguments and synchronization are preserved. This
CPU observer adds no GPU resource, command, wait, submission or application
rejection. The existing color capture producer and consumer still use their own
GPU resources and completion rules. Camera packets, shader admission, source
eligibility, leases, quotas and strict XR validators are unchanged.

The native CPU proof emits 46 model snapshots and 48 writer states across 31
SourceHistory prefixes. It covers raw values under unknown pipeline modes,
static/unknown binds, input mutation, reset/reuse, 80 discarded copies, separate
last-call and candidate origins, refused arrays, WithCount zero, overflow,
ignored GPL pointers, reviewed and unknown raster nodes, and fixed JSON budgets.
The model records 8,477 assertions; integration assertion counts include a
concurrent completion check and are specific to the saved execution.

On the tested native ABI, Recording remains 616 bytes; DrawSnapshot is 840,
PipelineSnapshot 904, RasterDeclaration 560, and the two raw fixed snapshots are
88/80 bytes. Strict Windows object builds separately assert these sizes; those
objects are not a local Windows execution. Existing registry, event and lease
quotas remain unchanged. These payload estimates exclude allocation overhead,
shared control blocks, other history and retained immutable generations; they
are not a total CPU-memory bound. The largest saved CPU state/prefix are
7,096/65,831 bytes, below their unchanged 16 KiB/128 KiB budgets.

The portable auditor keeps schema 1/2 results identical and validates schema 3
without promoting raw facts. It checks captured-value/null consistency, exact
form and origin/count metadata, epochs, same-bank interpreted consistency,
separate viewport/scissor setter ordinals and immutable declaration counts.
All 365 Python tests pass, including 41 auditor cases. The independent preflight
accepts 94 schema 3 states and 67 historical states. Its separate read-only review
rejects 18 related in-memory falsifications. Those review counts overlap the
main auditor controls and are not additive. Corrections found in that review
were made before any new physical execution.

Four owned X11/WSI runs passed with the observer off/on in native and Steam
Runtime environments. They preserve 512 original draw controls, with 16 schema 3
states joined to eight enabled delivered prefixes. All 67,108,864 original color
and depth reference bytes compared off/on matched, as did 4,194,304 delivered
pixel bytes against their same-process original sources. Validation reported
zero errors. The original device explicitly enabled the required DepthClip
feature and held its flags-zero structure and payload identical off/on; the
observer still reports the clipping value and effect as unknown.

The [owned CPU and WSI proof](proofs/raw-writer-arguments-v1.json) records frozen
source/binary hashes and the independent read-only reviews. It provides no game
result, sampled-resource identification or coverage claim.

The [separate actual Diablo trial](proofs/diablo-raw-writer-arguments-v1.json)
retains four successive monoscopic pairs, at reference and widened fields.
Seven content-matched writer draws have one WithCount viewport and scissor,
with raw and interpreted values from the same epoch. Each declares the complete
2560×1440 source, including the negative-height viewport. No central subrectangle
is declared by these parameters in those seven draws. This is not a proof of
effective coverage, initialization or the cause of the still-visible cutouts
and central rectangle. UV values, descriptor placement, the sampled image and
UBO contents remain unknown.

The game was closed through its normal UI and all owned processes exited;
original files and 262 named exports remain identical. The command monitor
ended with 143 and did not write its result file. A separate UMU child status 0
is retained as a different observation; no overall launcher status 0 is claimed.
No headset, OpenXR, game Khronos validation, final global fence/lease count or
DeviceDestroy event is claimed. The trial requested no depth readback or return
phase, and does not supply a common scene time or continuous immersive rendering.
