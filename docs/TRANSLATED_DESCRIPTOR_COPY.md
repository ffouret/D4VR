# Owned translated D3D12 descriptor copies

This control tests real D3D12 descriptor copies against owned pixel results and separately investigates which original Vulkan `vkGetDescriptorEXT` calls the diagnostic layer records. It follows the completed [Vulkan placement counterexample](DESCRIPTOR_WRITE_PLACEMENT.md). It uses a new offscreen executable and new owned HLSL, with no game shaders, game prefix, headset or XR submission.

**The owned pixel control passed; the original-query transport remains incomplete.** The third preparation produced 16 exact BGRA readouts across disabled/enabled observation. Its enabled journal retained zero calls but ended with one active instance and no device. This does not establish an absence of runtime queries or a descriptor-copy-without-query counterexample. The earlier failed preparations remain separate evidence. The [public proof](proofs/translated-descriptor-copy-v1.json) records both outcomes.

## Owned draw and descriptor operations

`native/tests/translated_descriptor_copy.cpp` creates two distinguishable 8×8 RGBA UNORM textures, point clamp/repeat samplers, a CPU-only SRV heap, a shader-visible SRV heap with one selected slot, a shader-visible sampler heap and an RTV heap. Resource and heap records describe the fixture's own construction and lifetimes. They are not a general live application catalogue.

The new `translated_descriptor_copy.hlsl` draws a fullscreen triangle. Its pixel shader samples at `SV_Position.xy / float2(256,144) + float2(0.125,-0.125)`, at LOD zero. It has no vertical coordinate flip. The target is 256×144 `B8G8R8A8_UNORM`, with full viewport/scissor, one sample, blending and logic operations disabled, and all four color channels writable. Texture B has variable alpha so both B and A can distinguish a wrong image.

All eight cases keep geometry, coordinates and the selected shader-visible image slot fixed:

| Case | Original owned D3D12 operation | Expected image / sampler |
| --- | --- | --- |
| `direct_A_clamp` | Create an A SRV directly in the selected visible slot | A / clamp |
| `direct_B_clamp` | Create a B SRV directly in that slot | B / clamp |
| `simple_A_clamp` | `CopyDescriptorsSimple` from CPU-only A to the selected slot | A / clamp |
| `simple_B_clamp` | `CopyDescriptorsSimple` from CPU-only B | B / clamp |
| `ranges_A_clamp` | `CopyDescriptors` with one explicit A source/destination range | A / clamp |
| `ranges_B_clamp` | The same range form with B | B / clamp |
| `after_bind_B_clamp` | Initially create A; copy B after heap/root-table/draw recording and command-list close, before execution | B / clamp |
| `direct_B_repeat` | Create a B SRV directly; select the repeat sampler | B / repeat |

Original D3D12 calls have paired BEFORE/AFTER journal rows with the same ordinal and arguments. Typed source resources, heap generations, descriptor increments, CPU/GPU handles, slots and copy ranges are recorded from the owned caller. `CreateShaderResourceView`, `CopyDescriptorsSimple` and `CopyDescriptors` return `void`: their AFTER rows record return, with no invented HRESULT or success code. APIs that actually return an HRESULT retain that result separately.

The fixture supplies explicit resource-state transitions, producer fences and readback completion. Its waits belong to the owned renderer. The observer adds no wait, submission, GPU command or resource operation. A manifest, the original-call journal, root constants and eight raw BGRA readouts are required for a completed run.

## Separate explicit Vulkan observer

`native/vulkan/descriptor_query_observer.*` and `descriptor_query_observer_layer.cpp` form the new explicit `VK_LAYER_D4VR_query_observer` Unix layer. Its standalone build is in `native/descriptor_query_diagnostic`; it changes no existing trace-layer hook. Activation requires explicit layer loading and `D4VR_DESCRIPTOR_QUERY_OBSERVER=1`. The off control loads the same layer with observation disabled and forwards original calls directly.

For an admitted original `vkGetDescriptorEXT` call, the observer freezes only the selected typed union input before forwarding, then copies only that call's returned output span afterward. It forwards the original device, info pointer, chain, size and output pointer once. Unknown chains or types stay unknown. Ignored sampler fields are not read for sampled-image descriptors; ignored format fields are not read for uniform/storage-buffer descriptors. A buffer address is an observed numeric operand, not a resolved allocation or resource identity.

The observer never issues an extra descriptor query or searches mapped memory. It does not intercept generic CPU copies. `vkGetDescriptorEXT` itself returns `void`; `original_returned` describes completion of that CPU callback, not a Vulkan result or GPU completion.

| Payload or transport bound | Limit |
| --- | ---: |
| Retained query records | 256 |
| Retained queries per selected case | 32 |
| Returned output bytes per record | 256 |
| Phase-file bytes | 256 |
| Tracked instance routes / device routes | 32 each |

Case zero is bootstrap: its queries increment counters without reading typed payloads or returned bytes and without consuming the 256-record budget. Cases one through eight may retain payloads. Oversized outputs are unavailable; the observer does not scan or retain a partial prefix instead. Per-case drops, unknown phases, changed phases, pending originals and observer faults remain explicit.

Loader routing is limited to this owned preparation's single common downstream chain. Exhausting the fixed route arrays faults observation and delegates unretained handles to the common downstream procedures, including wrappers obtained through GIPA. This is not a general unbounded router for arbitrary layer chains. A route overflow or chain mismatch prevents complete evidence; it does not authorize rejecting an application call.

Every header, query and footer keeps these facets false:

- `resource_generation_known`
- `selected_slot_placement_known`
- `current_contents_at_draw_known`
- `sampled_resource_GPU_read_proven`
- `exclusive_phase_attribution_proven`

## Phase labels and complete logs

The PE executable writes, flushes and closes a regular phase file before its owned API call, using `D4VR_QUERY_PHASE_1 <caseOrdinal> <apiPhase>\n`. The Unix layer reads the same file through its supplied POSIX path. Case ordinals are zero through eight; API phases are zero through six: control/binds, typed SRV creation, simple copy, range copy, copy after binding, execution/wait, and readback.

The layer samples the file before and after an original query. Equal labels are observations of a global CPU file, not exclusive attribution of callbacks or work on other threads. Changed, missing, malformed or unreadable labels cannot establish a complete phase. No PE-to-ELF function call is claimed.

JSONL carries a parent-pinned run nonce on the header, every query and the footer. It includes total/per-case calls, returns, retained records, drops and bootstrap counts. Final output is written at normal ELF runtime destruction; complete evidence requires instance/device teardown, and the first `DestroyInstance` does not prematurely close a process that later creates another instance. Missing or truncated final output, active routes, overflow or uncertainty prevents complete closure.

The disabled control produces a real disabled header/footer and no query records. Its zero counters do not prove that no original queries occurred. A complete enabled log can support only a bounded statement about the finite observed phase. It cannot prove that the runtime never queries descriptors or that a shader read a particular descriptor.

## Independent finite audit and isolation

`tools/translated_descriptor_copy_validate.py` checks exact original D3D12 arguments against the owned resource/heap tables and copy endpoint arithmetic, then validates observer accounting independently of the pixels. It derives the complete BGRA reference from rational pixel centers, explicit texture formulas and point clamp/repeat addressing. No fixture-produced reference image supplies expected pixels. Wrong-image, copy-source and sampler witnesses must discriminate.

The parent pins the executable, new DXIL, observer and private native D3D12/D3D12Core/DXGI preparation. Successful module-path snapshots before/after the cases must join those pins. Version labels and historical game logs do not attest which DLLs a new process loaded. A fresh private prefix/output is used; installed runtimes and game files remain unchanged.

The intended validation configuration includes Khronos synchronization validation and the translator's debug configuration. Any intentional translator startup validation self-test must be identified and counted separately from unexpected errors. No blanket claim of zero validation events is appropriate.

## CPU preflight and first failed preparation

The observer passed 22 CPU cases with 8,464 assertions in each off/on/I/O-fault run, strict Linux unit/ELF builds and strict Windows core/test object builds. An independent extension passed those 22 cases plus seven additional cases with 9,055 assertions, including reentry, invalid phase providers, zero quotas with poison pointers, 256/257-byte output boundaries and FIFO rejection. It used only mock original functions. All original arguments, return values and required loader-link advancement were checked without a Vulkan driver or Wine application.

The initial full Python freeze passed 533 tests. The final full suite passed 539 tests; the translated auditor's final suite passed 44 tests. Its revisions added native-DXGI dependency checks and the distinction between a pending BEFORE record and a completed AFTER record. A pending original remains incomplete; the parser does not invent an AFTER phase failure before that call has returned. A separate auditor correction accepts the original `GetCopyableFootprints` total of 1,824 bytes without requiring that total to be a multiple of the 256-byte row-pitch alignment. Totals below 1,824 remain refused. The pixel oracle and native renderer were unchanged. These changes and all source pins precede the accepted third execution. Strict Linux builds and Windows objects were checked locally; local CMake was unavailable. The separate CI target compiles the owned Windows D3D12 fixture without executing it.

Physical attempt one returned `D3D12CreateDevice` HRESULT `0x887a0004` and produced no readouts. Its log retained six `VUID-VkPhysicalDeviceProperties2-pNext-pNext` errors concerning ID properties and an unavailable enabled-extension contract. These are failed preparation evidence, not a successful control or an accepted debug self-test. The off observer remained disabled and its footer did not claim complete closure.

CPU preparation review confirmed that the private setup omitted the native DXVK DXGI dependency used by the inspected GE runtime path. That establishes a missing preparation dependency. It does not retrospectively attest the loaded DXGI module in the failed process or prove the exclusive cause of its HRESULT. The first attempt, its source pins and logs remain archived unchanged.

The second preparation added an explicitly pinned private DXGI dependency and native override, with the same renderer, shaders and observer. Its off execution exited zero and produced eight BGRA readouts; D3D12, D3D12Core and DXGI paths joined the new pins. The log also confirmed descriptor-buffer support. Execution success did not admit the validation result: the older validation layer reported modern structure/layout values it did not recognize, alongside draw-binding and other VUIDs. Those messages are retained and are not silently waived or classified wholesale as harmless. No on execution was launched for that preparation. The third preparation instead pinned an official Khronos validation nightly and manifest in a private directory, without installing or changing a driver. No accepted combined off/on certificate or query-absence conclusion is derived from attempt two.

A separate CPU review found no causal evidence that the observer's internal pre-initialization null GIPA forced Vulkan 1.0. Its mock showed the application's requested API version forwarded unchanged. The [public loader resolves global entry points](https://github.com/KhronosGroup/Vulkan-Loader/blob/v1.3.280/loader/trampoline.c) separately from this explicit layer's instance dispatch. This source/protocol comparison is not binary equivalence evidence for the installed Wine or loader.

## Third preparation: pixels passed, query transport incomplete

The fresh disabled/enabled pair completed all eight cases in each mode. All 16 BGRA readouts, totaling 2,359,296 bytes, matched the finite CPU oracle exactly. The eight corresponding off/on pairs were bitwise equal, comparing 1,179,648 bytes once. The direct A/B clamp witness differed at 73,152 B-or-A byte positions per run, and each simple/range/after-bind copy matched its corresponding direct image. This validates the owned translated copy and sampler pixel cases. It does not establish which internal CPU descriptor-copy mechanism produced them.

Reported module paths and the parent's before/after disk hashes joined all four required roles: fixture, D3D12, D3D12Core and DXGI. This is path/disk integrity evidence, not an independent hash of loaded process-memory bytes or proof of binary/source equivalence. All 12 physical source pins equal the final pins; no production or auditor source changed after the third execution.

The accepted checker was the official Khronos nightly targeting commit `8fa483ff4e2706601f4b1a1f5a4666b42673ba85`, reporting API 1.4.366. Its release asset SHA-256 is `7eb0ff0050fe60d06ff9335ece4d8de555046c930baa6f93d575d4faa87a2e96`; its validation module SHA-256 is `0adccf0db45a8c1df017516ff7bf8e32a8a05dbf9cad33e4c3082f610297c2cb`. The saved runs contain zero non-sentinel errors and two activation self-test markers. They also contain 16 loader warning lines, two pressure-vessel warning lines and four runtime OpenXR extension-query warning lines. No headset or XR rendering test was performed; absence of runtime OpenXR queries is not claimed. This does not certify complete dynamic descriptor-buffer shader-access validation.

A CPU review reconstructed the saved audit exactly with a second integer pixel oracle, without calling the production pixel functions. It rehashed 92 files before/after, checked all 16 readouts, refused 64 in-memory B/G/R/A corruptions and 14 rehashed metadata/log/module/source falsifications, and changed no capture. The reviewer authored the production auditor, but not the fixture, HLSL or physical runner; pixel-reference independence does not claim third-party independence from their own metadata parser.

The enabled saved query footer reports zero calls, one active instance, zero active devices and `coverage_complete=false`. Rendering and descriptor-buffer support are visible in the logs, which also show multiple loads of the layer. The JSONL does not identify its host PID or DSO epoch, so it cannot be joined to the renderer's device lifetime. An earlier loader scope, multiple processes sharing an exclusive output path, or separate DSO lifetimes are hypotheses, not established causes. Zero retained calls are not evidence that the runtime made no queries, including during copies. The disabled mode's zero counters likewise make no absence statement.

The next transport investigation must distinguish process/DSO/instance scopes with bounded, separately identified journals before interpreting a closed query window. It must preserve original forwarding and report missing scopes honestly. No observer repair or further physical run is part of this result.

The earlier game observation, numerical UV gates, game rectangle cause, effective coverage, rendering repair and XR submission remain unchanged and unestablished. The owned pixels do not promote selected-slot placement, current descriptor contents or GPU dynamic descriptor access to known facts.
