# Independent sampled-writer declaration facets

Schema 2 of the opt-in `D4VR_SAMPLED_WRITER_STATE=1` observation separates a
pipeline's push-layout, viewport and scissor declarations. A known declaration
does not make the entire pipeline, fragment shading rate or pixel coverage
known. This extends [capture-owned writer state](SAMPLED_WRITER_STATE.md) inside
the existing bounded prefix. The 176-byte camera packet, `sourceScaled` policy
and `SOURCE_CLIP_UNITS` helper are unchanged, as are the GPU capture, projection,
eligibility and completion rules.

The [owned evidence](proofs/sampled-pipeline-facets-v1.json) covers CPU controls
and four repository-owned X11/WSI runs. It contains no game trial. The original
[schema 1 evidence](proofs/sampled-writer-state-v1.json) remains historical and
unchanged; its saved observations are never upgraded to schema 2.

A [separate new game trial](proofs/diablo-sampled-pipeline-facets-v1.json) retains
four successive monoscopic pairs. Seven matched writer draws have compatible
push words; their conventional viewport/scissor declarations remain unknown
because of the precisely recorded depth-clip raster subchain. This observation
does not identify a sampled resource or repair the persistent cutouts.

| Facet | Required captured evidence | Meaning of a known facet |
| --- | --- | --- |
| `push_layout` | Complete immutable layout ranges, unique GPL owners and the existing exact push-range comparison | Saved fragment words can be checked against the original bound layout. It does not resolve a descriptor or prove a shader read. |
| `viewport_declaration` | A unique pre-raster owner and its consumed static/dynamic mode and count declarations | The recording can interpret the conventional viewport operand, subject to its setter, count and epoch knowledge. |
| `scissor_declaration` | A unique pre-raster owner and its consumed static/dynamic mode and count declarations | The recording can interpret the conventional scissor operand, subject to the same independent state checks. |

`pipeline_facet_known` contains these three booleans. Each is true exactly when
its bounded reason mask is zero. `pipeline_facet_reasons` lists the corresponding
reason names, while `pipeline_facet_reason_masks` records their fixed bit
assignments. Structural ambiguities propagate to every dependent facet. An
unsupported raster subchain can leave push compatibility known while the
viewport and scissor declarations remain unknown. An unknown creation chain,
unreviewed flag, incomplete library list or ambiguous GPL ownership stays a
conservative refusal of the affected metadata. No application call is refused.

The newly reviewed rendering flag is
`VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR`
(`0x00200000`, with its flags2 equivalent). It no longer vetoes exact push-layout
or conventional viewport/scissor facts by itself. Its own value remains visible
in `pipeline_recognized_rendering_flags`; inherited values are recorded
separately. This is a specific classification, not a blanket admission of other
rendering flags or pNext structures.

The flag declares intended use with a fragment shading-rate attachment. It does
not identify an active attachment or its texels. A non-null dynamic-rendering
rate attachment requires the flag; the converse is not established by that
requirement. These are the [Khronos pipeline and draw contracts](https://github.com/KhronosGroup/Vulkan-Docs/blob/v1.3.275/chapters/commonvalidity/draw_common.adoc#L1126-L1133).
Push compatibility remains based on the original push ranges, as specified by
the [pipeline-layout compatibility contract](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#descriptorsets-compatibility).

The parser also recognizes the presence of
`VkPipelineFragmentShadingRateStateCreateInfoKHR` and the
`VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR` declaration as independent rate
metadata. It does not read the static structure's payload or claim a dynamic
rate value. The pinned [GPL ownership contract](https://github.com/KhronosGroup/Vulkan-Docs/blob/v1.3.275/chapters/pipelines.adoc#L1596-L1686)
places viewport/rasterization state in the pre-raster subset, the static KHR rate
structure in both pre-raster and fragment subsets, and the attachment-flag
declaration in the fragment subset. Ignored link-only bodies are not read.
`fsr_static_declared_scope` is `own_parts`; inherited rate reasons remain
explicitly separate from that local presence flag.

`pipeline_declarations_complete` keeps the scope
`legacy_header_admission_summary`. It is not a complete pipeline certificate and
remains false when the recognized rate declarations are present. Likewise,
viewport/scissor `effective_known` has the scope `single_conventional_operand`:
it checks the known operand, single count and the model's conventional validity
predicate. It does not establish effective raster state. The serializer keeps
`fragment_shading_rate_known`, `effective_raster_state_known`,
`effective_write_coverage_known` and `coverage_known` false. Existing command,
rendering-extension, resource, ordering and incomplete-prefix gaps are retained.

Creation diagnostics save at most 32 pNext type labels, with the first eight
unrecognized types and their bounded count. The pre-raster parser retains at
most 64 dynamic declarations, eight rejected type labels and eight labels per
dynamic/raster/viewport subchain. Truncation and duplicate/refusal reasons remain
visible. Each `*_info_observed` flag means the corresponding valid structure
body was actually inspected; `*_info_absent` means its null pointer was actually
checked. Both false means ignored or not reached. An empty array alone never
establishes the absence of an extension. WithCount can therefore leave an
ignored viewport structure unobserved without guessing its contents.

The original pipeline snapshot is captured before the driver call and published
for actual non-null outputs. A draw retains its immutable declaration value
through a const shared reference, alongside its existing immutable layout
references. Later input mutation, catalog destruction, reset or handle reuse
cannot replace that generation. Serialization uses these captured values,
without a current registry lookup. The serialized prefix does not pin those
model objects. Push bytes, compatible words, setter/count epochs, original CPU
submission results and producer-fence completion remain separate observations.

The CPU model passes 8,425 assertions and emits 24 snapshots. Its causal 2x2
control changes only the descriptor-buffer and reviewed FSR flags while keeping
the layout, push bytes and conventional operands identical. Other controls
cover independent raster/creation gaps, unilateral dynamic-mode conflicts,
ignored GPL pointers, a real 8,192-layout admission cap, 80 discarded recordings,
input mutation, reuse, overflow and bounded JSON fallback. SourceHistory adds
22 integration cases. Strict native builds and three Windows object builds pass;
the Windows objects were not executed. The optional local sanitizer attempt
could not link the host's missing ASan runtime, so this local CPU bundle claims
no sanitizer execution. Publication CI validation is tracked separately.

On the tested ABI, fixed payloads are 616 bytes per recording, 672 per draw,
456 per layout and 872 per pipeline snapshot. The current registry remains
65,536 entries, recordings 32,768 and events 8,192. A registry entry bound is not
a bound on every pipeline snapshot lifetime: recordings and draws can retain
removed immutable snapshots. Estimates exclude allocator and map overhead,
control blocks, JSON bodies, other history and concurrent in-flight creation
temporaries. No total process-memory cap is claimed. The 32-byte fragment bank,
32 ranges per layout, 16 KiB per-draw JSON and existing four-payload,
64-event/four-occurrence/128 KiB prefix limits are unchanged.

The four physical controls use native Vulkan and Steam Runtime, each with the
state observer off and on. Both configurations query and explicitly enable the
attachment-FSR feature and use the same application pipeline flag `0x00200000`.
There is no rate attachment, static rate pNext or dynamic rate declaration in
this flag-only workload. Unsupported extension/feature configurations cannot
pass this control. Descriptor-buffer support remains enabled for the existing
CPU descriptor queries; it does not implicitly add a descriptor-buffer flag to
the application pipeline.

The runs save 512 original application draw controls. Only eight observer-on
delivered prefixes carry the sixteen draw states compared with those controls:
eight static, four regular dynamic and four WithCount. All 128 argument records
per run are identical across off/on and both environments after removing only
the four process-local command/pipeline/layout handles. Off/on original color
and geometric-depth references compare 67,108,864 bytes; the sixteen delivered
color images compare 4,194,304 bytes against their original source frames.
Khronos synchronization validation reports zero errors. Selected receipts cover
classic push, PushConstants2KHR and core count setters; EXT count aliases occur
in the original control logs and CPU tests, rather than those selected receipts.

The certificate separates the source hashes used during those physical runs
from the later read-only auditor and test hashes. Auditor hardening is validated
against the unchanged saved positive files and in-memory falsifications; it
does not imply another physical execution. The owned fragment shader does not
read the observed fragment push constants. These results establish bounded CPU
declaration retention, not sampled resource selection, UBO contents, shading-rate
effects, last GPU writer, coverage repair, common scene time, XR pose,
SteamVR certification or continuous immersive VR.

The final auditor recomputes conventional validity from the raw viewport bits
and scissor integers. It checks that a known push word comes from an aligned,
complete update covered by the saved layout's ranges and stages. An
unobserved structure cannot retain parsed types or a parsed dynamic array.
These checks harden the certificate without changing native code or saved
execution inputs. All four new FSR audits and four historical schema 1 audits
remain exactly equal to their original results. The final auditor has 27 unit
tests within the passing 351-test portable Python suite; eleven in-memory
alterations of real positive receipts are refused. A separate CPU review
confirms the three correction families with nine related falsifications; those
counts overlap and are not additive.
