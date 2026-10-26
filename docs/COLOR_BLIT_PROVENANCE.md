# Original BLIT identity carried by the color capture

The presentation producer now carries a value receipt from the original BLIT
through its private copy, CPU snapshot or GPU lease, and projected eye receipt.
This identifies the actual input image of an exported capture. It does not
identify every earlier pass that contributed to that image or prove a shader
sampled a depth descriptor.

## Frozen values and completion

`SourceBlitObservation` stores the original observer namespace, source and
destination image lifetimes, command recording and common ordinal, plus the
actual raw image arguments, region, layouts and filter. Raw handles are values
from that call, not lifetime identities or a lookup performed at export time.

The two CPU submission counters remain separate. `historySubmission` labels
SourceHistory's original BEFORE/AFTER occurrence; `captureSubmission` retains
the existing camera/capture counter. Neither is a GPU timestamp. Successful
original submission populates them once. The private producer's existing fence
must complete before a delivered receipt reports `producer_complete` and
`bridge_known=true`. Missing identities or counters keep the bridge unknown.

Repeated or ambiguous command occurrences invalidate the bridge rather than
relabeling its original submission. Reset removes the command lookup while
retaining inserted resources until their existing lifetime rules permit reuse.
Copies returned by value, including prior leases and receipts, remain unchanged.
The original one-time-command eligibility and all queue/fence ownership rules
remain in force; this metadata does not make arbitrary reusable recordings safe.

`HistoryBlitRef.eventRetained` describes journal retention independently. A hook
can freeze known original labels even when narrow admission or a full event
budget excludes its journal node. A true retention flag does not guarantee that
node appears in an exported bounded window. Neither case supplies an invented
node or a complete resource graph.

Default captures remain usable without SourceHistory, with an unknown identity
bridge. Coherent XR validators and the explicit successive diagnostic mode are
unchanged. No old image receives a current pose, time, scene or resource label.
No GPU command, allocation, wait, submission or application rejection is added
by carrying this metadata.

## Layer receipts

The layer obtains the original BLIT reference before its private capture records
commands. Both legacy and core/KHR BLIT paths forward the original call with
unchanged arguments. Accepted captures emit bounded
`presentation_blit_recorded` and `presentation_blit_submitted` rows, at most
32 of each per device. Missing rows cannot be treated as proof of absence.
Snapshot and diagnostic/projected pair JSON include the copied `source_blit`.
The existing producer fence supplies its completion status; a record-time row
alone is not a delivered image.

## Recording pressure

Consumer SourceHistory previously allocated unrelated rendering and transfer
events before its window filter. Live command recordings could pin the entire
node budget even after the rolling journal was evicted. Admission now allocates
only configured depth candidates and bounded live endpoints reached by declared
attachment association or directed transfer. Up to 64 additional endpoints are
retained; handle reuse does not inherit an earlier lifetime's admission.

Reverse transfer observations preserve their direction without admitting an
unrelated upstream source. Unknown routes keep command gaps. Excluded attempts
and recording/live-node/reference/endpoint-limit overflows are reported
separately. Exclusion is a deliberately unobserved scope, not an unused resource.
The event, occurrence, query, serialization and GPU-resource limits are not
increased. Prior journal eviction still conservatively marks missing prefixes.

## Independent controls

Sixteen CPU cases pass, including many unrelated live command recordings before
three target captures, directed transfer and lifetime reuse, exhausted-budget
hook identity, and endpoint caps. A pressure control excludes 3,200 unrelated
allocation attempts with zero event nodes or overflow before the targets; its
three target windows retain their queries, color declarations and BLITs.
This is an owned causal pressure control, not a breakdown of earlier game
counter causes.

Mocked presentation tests cover twins, reversed endpoints, mutation after
recording, delayed producer fences, omitted failed-submission callbacks,
ambiguous occurrences, missing independent counters, reset, slot reuse and
unchanged prior receipts. Strict projection tests retain their previous rules.

Four real X11/WSI controls compare identical original application work with
consumer observation off/on, natively and in Steam Runtime. A separate read-only
`tools/blit_provenance_validate.py` compares exported pixels and actual app BLIT
arguments against the frozen record/submission/delivered receipts. It launches
no process or GPU work. Consumer-off captures remain generic with zero exact
History bridges. Game observations, physical SteamVR and complete scene
coverage require their own evidence.

The [owned proof](proofs/color-blit-provenance-v1.json) records eight exact
original BLIT bridges in the enabled physical runs and eight generic captures
in the disabled controls. Independently compared exported pixels total
4,194,304 bytes; the full original application references compared off/on total
67,108,864 bytes. Khronos synchronization validation reports zero errors.
No matching BLIT journal node is available in those exported depth windows;
the node is not fabricated from a true retention flag.

## First actual game result

The [new Diablo observation](proofs/diablo-color-blit-provenance-v1.json) completes
six successive monoscopic pairs and three D24 exports. All twelve delivered eye
receipts carry original source values, identifying three alternating lifetime
labels in this one session. Four eyes can additionally be compared to both
recorded and submitted trace snapshots. The 32-row limits were exhausted by
unexported capture work, so eight eyes have only their delivered receipts; no
secondary hook chain is claimed for those eight.

At the last capture the four instrumented recording/live-node/reference/endpoint
overflow categories are zero, with 3,586 live nodes of 8,192. A cumulative
37 other or unattributed overflows remain. The earlier session's 5,612,508 is a
counter from a different duration and activity, not a normalized rate or proof
of pixel improvement. Missing prefixes and excluded scopes remain explicit.

Each depth window retains eight CPU occurrences and 100 events. None relates
its depth-associated images to an exact captured source lifetime. That is a
remaining observation gap, not proof that depth is unused. The next targeted
writer examination can start from these actual source labels rather than from
similar image dimensions. Trace retention should also follow delivered leases
so discarded work cannot exhaust their secondary evidence budget.

The game exits through its normal UI; original files and 262 named exports
remain identical. Wide-field cutouts and the central rectangular color defect
persist. There is no independent original-game argument/pixel reference,
Khronos game validation, common scene instant or headset submission in this
experiment.

The published layer also contains a CPU exception guard for secondary submit
reporting. That final guard is checked on the owned WSI/CI path; the game
experiment records the earlier layer hash explicitly and was not repeated for
this reporting-only change.


## Lifecycle values retained with delivered images

`SourceBlitStages` now saves two full values inside each producer copy. The
recorded value is frozen at acceptance, before any original submission succeeds.
The submitted value is frozen by the first matching successful original-submit
callback, before the producer fence completes. The final observation keeps its
separate completion or ambiguity state. A later duplicate occurrence does not
rewrite either earlier value or its counters.

Snapshots, GPU leases and both projected-eye receipt types carry these values
directly. Delivered JSON includes `source_blit_stages`, with explicit knowledge
flags; unknown stages retain an absent default value. The capped secondary
trace rows remain optional corroboration. Their exhaustion by discarded copies
cannot erase the selected image's saved stages. These saved values are still
producer observations, not independent application observations or GPU times.
The existing producer fence and original success remain required for completion.

A CPU dispatch mock holds a selected image across 80 other consumer-rejected
and retired copies, delayed fences, command resets, source-handle regeneration
and private-slot reuse. The selected and previously returned values stay exact.
The stage payload adds 392 bytes per copy on the tested ABI, at most 1,568 bytes
inside the existing four-copy pool; this is not a global CPU memory cap. The
getters copy at most four values without Vulkan calls.

The read-only auditor's `--require-stages` option verifies these delivered values
against original application BLIT arguments and exported pixels even when
secondary rows are absent. Available secondary rows must still match. The old
mode remains available for earlier proofs. Falsifications cover edited phases,
namespace/counter conflation, absent original success and invented completion.

## Admission from the actual accepted source

After Capture accepts an original BLIT, the layer can admit its exact live source
into the existing SourceHistory endpoint budget. The observer checks its own
session, the current command recording and ordinal, a last-original-BLIT marker,
and both raw endpoints' live lifetimes. It never admits a similar-sized twin,
a reused lifetime or a resource number from another process. The mode remains
opt-in and shares the unchanged 64-additional-endpoint limit.

This admission affects subsequent observed commands only. It cannot reconstruct
writers excluded before that hook or change the old `eventRetained` flag.
The subsequent [capture-owned writer prefix](CAPTURED_WRITERS.md) now freezes
source operations against the original submission and survives the selected
lease. That page records its separate owned controls and new Diablo trial.
Neither metadata addition repairs the wide-field cutouts or establishes headset
delivery.

See the [lifecycle proof](proofs/blit-lifecycle-v1.json) for the new owned controls.
