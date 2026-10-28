# Closed selected original-call relation

`tools/closed_call_relation_validate.py` adds a separate saved-fact calculation.
It does not change the strict owner audit, native observer, fixture or runtime.
`analyze_closed_call_relation` consumes the same complete claim/journal set,
original PE receipt, nonce and source-build declaration as the strict analyzer.
Its purpose is `closed_original_nonce_call_relation_audit`.

The new fact `closed_selected_original_call_relation_known` describes only two
explicit original nonce-marker/query pairs and their closed target lifetimes.
It is not full runtime or renderer ownership. The complete old result is kept
unchanged under `legacy_strict_result`; the producer's footers, refusal reasons
and `coverage_complete` values are never rewritten.

## Admission

The unchanged strict parser must validate every supplied schema-3 scope. No bad
foreign journal can be filtered out. The full claim set and external source pins
remain caller responsibilities. The calculation requires:

- Exact original PE activation and twelve BEFORE/AFTER edges, unique native
  matches for both names and queries, their typed arguments and returned bytes.
- A common target PID/start cookie/DSO epoch, device generation, sampler
  generation and parent generation. Marker/query ordering is original CPU
  entry/return ordering, never a GPU timeline.
- Successful known sampler creation and explicit sampler, device and parent
  destruction returns, in that order after the second query.
- No fault, unknown generation, pending original, overflow, dropped row, unknown
  selected phase or incomplete call/return accounting in any saved scope.
- Zero active devices and samplers in every scope. Each instance allowed to
  remain open must have its own complete successful creation, no pending
  destruction, and a different exact scope/generation from the selected parent.
  Open instances must also fit the producer's existing 32 live instance routes;
  complete-looking counters cannot invent a larger non-faulted producer.
  The same capacity also bounds routes necessarily held between each original
  creation return and destruction entry. This is a lower bound, not an exact
  runtime peak: a route can be removed before its destruction AFTER is recorded.
  With a pending lifecycle or a replay gap, these scalar diagnostics remain
  explicitly unknown and the selected relation is refused.

No application/engine name, enumeration pattern, process ordering or presumed
Wine cache behavior admits an unrelated instance. Completed raw-handle reuse
remains a new generation and cannot retag the selected occurrence. A missing
target destruction still refuses the new relation.

An unrelated original callback with complete accounting does not erase an
already completed selected call. An unknown, pending or lost callback refuses
this deliberately restricted first admission. Neither decision proves the
absence of callbacks outside the saved observation.

## Separate results on the existing owned control

The previously saved enabled Wine control contains two exact selected pairs.
Its target sampler, device and parent have explicit teardown records; two other
instance occurrences remain open, with no recorded device descendants. Their
origin is not inferred. The new calculation can admit the selected relation
while retaining both of the following old facts as false:

- `legacy_strict_result.original_call_owner_known`
- `legacy_strict_result.all_saved_scopes_complete`

The additional `all_saved_scopes_lifecycle_closed` reports global saved-lifetime
closure separately and remains false on these data. This is a new report and
source revision produced after the physical experiment. The old audit, claims,
footers, PE receipt, source archives and physical certificates remain immutable.
No Wine, Vulkan, game or headset execution is needed to compute this report.

## Limits

Inputs retain the old finite schema/claim/query/lifecycle quotas. The analyzer
does not open files, inspect processes, read memory or execute a runtime.
The complete new report is limited to 32 MiB of compact ASCII JSON encoded as
UTF-8. An incremental encoder checks all fields, including the full legacy
result. Exceeding this CPU output budget raises `ValueError`; it does not return
a truncated report or alter any input. This is a new report bound, not an
increase to runtime capture quotas or a bound on total Python memory.
Input facts and loaded source/binary identity are not attested by the model.
Opaque return bytes are compared only against their own original query; their
equality does not identify a resource or its contents.

All module backing, loaded-memory, renderer ownership, general handle conversion,
descriptor placement/current contents, sampled GPU read, exclusive phase,
global query absence, game input, coverage repair and XR submission claims
remain unproved. The old process/backing gate is not bypassed. This calculation
grants no rendering authority and cannot turn an old zero-query log into an
absence certificate.

A typed D3D12 copy receipt is a separate next step. It must distinguish the
original typed copy call and its last observed typed source from actual source
contents, which can still be unknown after a silent overwrite. CPU descriptor
handles are opaque tokens, not readable mapped-memory pointers.
