# Bounded source image use observations

**D4VR_SOURCE_CONSUMER_HISTORY=1** enables an optional CPU observer alongside
SourceHistory. It adds descriptor image query receipts, declared color attachment
associations and directed blits. It does not observe descriptor placement,
selected descriptor indices or actual shader reads.

The layer forwards original calls, pointers and results. This observer issues no
GPU commands, queue submissions or waits, allocates no GPU resources, reads no
mapped descriptor memory and rejects no application calls. Exceptions latch an
observer fault; limits produce explicit truncation or missing evidence. The
default Limits::consumerWindows=false retains the existing source prefix mode.

## Image identity

ImageUseRef is a value in one observer namespace: observerSession, image
lifetime, optional view lifetime, aspect/mip/layer range, and separate lifetime
knowledge and metadata completeness.

These labels are not Vulkan handles, another tracker's IDs, camera sequences or
GPU timestamps. Equal dimensions do not establish identity. A stale view lookup
preserves its original image label but reports the image unknown; it never
acquires a reused handle's new image. Unmodeled creation chains keep metadata
incomplete even when the lifetime is known. Aliasing and complete visibility
remain unknown.

## Descriptor queries

descriptorImageQueryBefore freezes the typed view, layout, chain completeness
and requested size before the original vkGetDescriptorEXT.
descriptorImageQueryAfter copies the original opaque output for that exact
token. It never resolves the view again. Duplicate or foreign callbacks cannot
replace the first completed output or retag its lifetime.

Only images matching the configured SourceHistory format and footprint enter
the query cache. The hook supports sampled images, storage images, input
attachments and combined image/sampler inputs. Sampler state remains unknown.
Unknown views are counted without inventing an association.

The cache can retain queries made before the target COPY. Each query records the
number of CPU submission observations preceding its BEFORE and AFTER callbacks;
membership in a forward report does not mean the query happened after COPY.
Exact completed receipts may be deduplicated only with matching lifetimes,
type, layout and opaque bytes. Application calls are counted separately.

The complete field means the CPU query callback returned. bytes_complete
separately indicates that its whole output fitted the bound and was copied.
Null output, size mismatch or excessive size does not become a partial exact
encoding. Every query retains these explicit limits:

    {
      "descriptor_selected": false,
      "gpu_read_proven": false,
      "placement_known": false,
      "sampler_state_known": false
    }

A query is not a binding or a dependency edge to a nearby draw. Descriptor buffer
addresses and offsets would also be insufficient to prove which encoding was
placed in a slot or selected by a GPU-dynamic index. Classic descriptor set
contents and update/push/template routes are outside this observation.

## Declared attachments and transfers

Up to eight color attachment declarations are retained with view/range, layout,
load/store, clear color bits and declared resolve endpoints. associated_image
and associated_view identify the depth-or-stencil attachment declared in that
same rendering scope. This is a declared association, not proof of a depth test,
fragment execution, effective write or initialized pixel.

The existing depth-first fallback to stencil is retained. A distinct stencil
attachment alongside depth and complete depth/stencil resolve semantics remain
unknown. Color resolve endpoint metadata does not certify execution or complete
semantics. Legacy attachments, secondary commands, generated commands, alias
routes and effective write state retain explicit gaps.

Intercepted vkCmdDispatch and vkCmdDispatchIndirect leave a write_route_missing
gap in consumer mode. Compute resource use, other compute stages and dispatch
forms that are not intercepted remain uncertified.

Each blit_image event freezes both image lifetimes, both subresource ranges,
both pairs of offsets, layouts and filter. Region count is bounded at sixteen.
Direction is preserved, including reverse blits. HistoryBlitRef identifies the
original command's observer, endpoints, recording and common ordinal; it is a
different type from HistoryCopyRef.

Blit interception is independent of presentation capture early returns.
Swapchain images share the observer registry, with origin="swapchain" and their
observed swapchain lifetime/flags. They are not described as vkCreateImage
results. Successful repeated enumeration preserves identities. Destruction
removes only lifetimes owned by that chain, not an unrelated CreateImage
lifetime. Metadata comes from the actual swapchain creation parameters.

The color capture receipt has not gained a resource identity bridge:
presentation_receipt_resource_bridge_known remains false. A WSI endpoint in a
blit observation must not be presented as an exact association to a particular
exported color receipt without that additional bridge.

## Forward CPU windows and bounds

A window is keyed by the exact five fields of a selected HistoryCopyRef.
Its initial resource identities are both source and destination. Declared
attachment associations and directed transfers may retain further candidate
endpoints; an unlinked color resource or nearby query is not sufficient.

The first counted occurrence is the target COPY's CPU submission, including only
commands/events after that COPY. At most eight CPU BEFORE occurrences are
counted, even if some contain no relevant event. Their order across queues is
CPU observation order, not GPU execution order.

Command/resource values freeze at BEFORE. AFTER updates only the completion
of that exact original occurrence, including failed and reversed returns.
Reset, resubmission and handle reuse cannot rewrite an old recording. A closed
window accepts no new occurrences or queries. A pending callback may still
complete its already retained exact token.

| Bound | Maximum |
| --- | ---: |
| Configured events per prefix or forward window | 512 each |
| Forward CPU occurrences per window | 8 |
| Retained reports | 8 |
| Live event nodes / snapshot references | 8,192 / 32,768 |
| Candidate query cache / query receipts per report | 64 / 16 |
| Opaque output bytes per query | 256 |
| Followed candidate image labels per window | 64 |
| Swapchains / swapchain image registrations | 16 / 64 |
| Serialized report bytes | 8 MiB |

With consumer mode enabled, each prefix and each forward window receives at
most min(configured cap, liveEvents / (4 * reports)) event references. Their
combined worst case retains at most half the live event node budget, leaving
headroom for new rolling history. The runtime uses three reports, so both
effective event caps are 512 at the default 8,192-node limit. Smaller test
budgets can lower these caps; JSON reports configured and effective values.

The query cache and report references together bound retained records at
64 + reports * 16, each with at most 256 output bytes. Payload accounting
excludes allocator overhead and borrowed pipeline catalog metadata; it is not a
hard process heap bound. Serialization overflow returns a small valid truncated
JSON envelope.

The bounded global query catalog is export-time cache context. Per-window query
snapshots contain only the target source/destination lifetimes, including earlier
cached queries. Evictions and truncation mean **not observed**, not **unused**.

## Validation scope

source_consumer_tests.cpp exercises the CPU contract: equal-size twins, unbound
queries, immutable opaque outputs, reuse, reset/resubmit, reversed AFTER,
directed blits, bounds, faults and WSI lifetime origins. Forwarding and owned WSI
fixtures separately check that the observation path preserves application calls
and outputs. This document does not itself certify a GPU run.

None of these observations establishes an actual sampled consumer, last GPU
writer, complete scene dependency graph, common depth/color time, initialization,
XR submission, culling cause or missing-pixel cause.

The [owned execution proof](proofs/source-consumers-v1.json) records four real
X11/WSI runs, natively and in Steam Runtime. Both controls retain the same
application descriptor features, eight CPU queries and original GPU workload;
only the new observer changes. The 67,108,864 bytes compared against their
same-environment controls are identical, with zero Khronos synchronization
errors. Four unique closed forward windows retain eight CPU occurrences and
26 events each: subsequent source clears, declared color associations and
color-to-swapchain blits. These remain declared candidate paths.

Eleven CPU cases pass 718,861 structural assertions. A separate read-only
auditor checks the snapshots, exact COPY bridges, native outputs and original
descriptor bytes. The dedicated forwarding test exercises absent, active and
faulted observers without changing the application pointers or outputs.
Forced handle reuse is a CPU control; the physical twin images use the handles
actually returned by Vulkan. No executed sampled shader is claimed.

`tools/source_consumer_run.py` runs the owned application controls with explicit
`--fixture`, `--compiler`, `--validator`, `--layer`, `--validation-layer`,
`--unit`, `--forwarding`, `--runtime` and a fresh `--output` directory. Its unit
argument is the existing source-history test binary. The separate
`tools/source_consumer_validate.py` reads those captures and the new consumer
unit's `--unit-report`/`--unit-log`, plus `--forwarding-log`; it launches no process
or GPU work. Runtime paths and raw reports stay private. Native Windows and
Linux sanitizer execution are verified by CI separately from these physical
Linux WSI runs.

## First game observation

A [new fixed monoscopic Diablo session](proofs/diablo-source-consumers-v1.json)
uses the validated layer and the existing shader bundle. Three exact COPY
bridges identify the source and destination in that same observer namespace.
Four cached original query receipts identify two source depth queries, one
source stencil query and one destination depth query. All predate the COPY
observations, return four bytes each and have incomplete view metadata. They
are not four executed consumers or four reads after COPY.

The three closed windows count eight CPU occurrences each and retain
114/114/75 events, including 13/13/12 declared color associations. None retains
a BLIT. These short windows do not prove absence of later transfers. Global
history reports 5,612,508 cumulative overflows and missing prefixes, despite
no local window truncation or observer fault. This reveals a recording coverage
limit that needs narrower selection; it is not a game GPU error count.

Six successive monoscopic color pairs and three raw D24 exports preserve their
own camera packets. Wide-field cutouts remain visible. The game exited through
its normal UI, its launcher returned zero, all owned processes exited and the
original audit plus 262 named exports remain identical. No headset or OpenXR
was used, no Khronos game validation or final fence/lease count is claimed,
and no sampled resource path or exported color receipt bridge is established.
