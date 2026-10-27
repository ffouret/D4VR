# Scoped original-query journals

This diagnostic gives each admitted process/ELF-library lifetime its own bounded journal. It addresses a transport limit in the [translated descriptor-copy control](TRANSLATED_DESCRIPTOR_COPY.md): that control's pixels passed, but its first exclusive output file could not be joined to the renderer's device. The old capture and its incomplete query conclusion remain unchanged.

**The owned native loader witness passed.** The accepted disabled/enabled executions returned four original queries; the enabled journal joined its two queries to the exact owned device lifetimes and returned bytes. The [public proof](proofs/scoped-query-journals-v1.json) records this finite positive control. It establishes no translated query absence, game resource identity or rendering repair.

## Claims and finite storage

The explicit Unix layer reads `D4VR_QUERY_OBSERVER_DIRECTORY`, a fresh parent-owned directory with mode `0700`. It opens every path component without following symlinks, checks the final owner/mode and keeps a directory file descriptor. Supply a canonical path from the execution host: on the tested host `/home` is a symlink, so the host's real path is required. The phase file lives outside the dedicated journal directory.

A runtime atomically claims one of eight fixed `slot-N.claim` names with exclusive creation. A claim contains the parent run nonce, slot, host PID, bounded `/proc/self/stat` start cookie and DSO epoch. The epoch is the persistent slot plus one within that nonce. Claims are never removed, recycled or evicted. The matching `slot-N.jsonl` also uses exclusive creation. A preexisting report is preserved; a failed or partial claim remains evidence of an incomplete scope.

The header is written immediately. Normal library destruction appends saved lifecycle/query rows and a footer. A crash can leave a claim and header without a footer; it cannot turn into a completed observation. The ninth claim attempt creates a fixed `overflow` marker when possible and faults observation while preserving original forwarding.

| Bound | Per journal | Across eight admitted journals |
| --- | ---: | ---: |
| Retained queries | 32 | 256 |
| Retained queries per selected case | 4 | 32 |
| Returned output bytes per query | 256 | — |
| Lifecycle records | 256 | 2,048 |
| Live instance / device routes | 32 / 32 | — |
| Exact physical-device mappings | 128 | — |
| Physical handles per original enumeration | 32 | — |
| Phase-file bytes | 256 | — |

Case zero remains counter-only bootstrap. A missing scope, overflow, drop, pending original, unknown phase or incomplete lifetime cannot establish query absence. An empty closed journal describes its own scope. It does not cover an unclaimed process or ELF namespace.

## Original lifetimes and query joins

Schema 2 repeats the run nonce, host PID, start cookie, slot and epoch on every row. A declared build ID joins external source/module pins; that string is not a hash of loaded process-memory bytes or proof of source/binary equivalence. Without the build macro, the ID remains explicitly unknown.

The layer records original instance/device creation and destruction by value. One common counter assigns unique entry and return transitions, including reversed or overlapping returns. BEFORE fields stay frozen; AFTER is single assignment and adds only defined original result/output facts. Failed or pending creations do not establish a live lifetime. Application and engine names are at most 64-byte prefixes, with explicit truncation flags, and never serve as complete identity.

An original `vkEnumeratePhysicalDevices` return establishes the exact physical handle's parent instance generation. A dispatch-table key shared by two handles is insufficient. Other enumeration paths that are not intercepted leave parentage unknown. Each admitted query captures its original device generation and parent instance generation at entry. Reused raw handles receive new generations; export does not look them up again in a current catalogue. These labels identify observed instance/device lifetimes and do not assign a sampler or image view a resource generation. Query and destruction intervals must agree before the auditor accepts finite coverage.

The observer copies only the selected legal typed input before the original `vkGetDescriptorEXT`, then that call's bounded returned output. It adds no query, resource operation, submission or GPU wait and does not scan mapped memory. Loader-link advancement is the required forwarding protocol. Exhausted routes and I/O faults close evidence without authorizing an application rejection.

A forked child closes the inherited output descriptor and does not retag or emit under the parent's PID. Its common downstream calls continue without entering inherited observer mutexes. This does not certify arbitrary driver behavior after fork, and the child's unadmitted scope stays unknown.

## Independent audit and CPU checks

`tools/query_scope_validate.py` validates claims, counters, creation/enumeration/query/destruction transitions and the exact returned payload. A caller-owned manifest must join host PID/start cookie, raw handles, original typed operands and returned bytes to a unique observer lifetime. The fixture's generation labels are not assumed equal to observer generations. Missing or ambiguous owner evidence blocks the join. Even a successful join proves no universal query absence or exclusive phase attribution.

Strict local Linux builds and Windows core/test objects passed. The forwarding suite passed 25 cases and 8,743 assertions in each off/on/I/O-fault mode. The Unix scope suite passed 12 cases and 224 assertions, including concurrent processes, ordinary reloads, independent `dlmopen` namespaces, eight claims plus a ninth forwarding call, handle reuse, immediate headers, inherited fork descriptors and refusal of symlink/wrong-permission paths. Eight quota journals retained exactly 256 records, with 32 per case globally, from 320 mock original calls; 64 excess records were explicitly dropped without eviction.

A separate reviewer added four CPU modes and 163 assertions, including distinct handles that deliberately share dispatch keys. The exact parent-built ELF also passed the 12-case mock lifecycle suite. The initial auditor passed 45 unit tests, independently accepted 25 final native CPU scope journals and marked two deliberately partial artifacts incomplete. Its pre-execution cross-loader revision passed 47 tests; the final full Python suite passed 586 tests. The revision preserves separate application/layer physical-handle and creation-output-slot domains, without inventing a mapping between them. Exact logical-device, sampler, query-output and lifetime witnesses remain required. These checks use mocks or saved files and establish no driver execution.

The explicit `--no-new-namespaces` test adapter passed 11 cases and 214 assertions. It omits only the namespace case and reports that omission. The normal namespace test was executed without instrumentation. Local sanitizer linking was unavailable because the host ASan library was missing; no local ASan/UBSan pass is claimed. CI may instrument the remaining cases with the explicit adapter. Windows objects were compiled locally, not executed locally.

## Owned loader witness and remaining limits

`native/tests/query_scope_loader.cpp` is the owned witness: one original Vulkan 1.3 instance, two simultaneously live logical devices and two sampler descriptor queries. It records its own PID/start cookie, original arguments, outputs and teardown. It creates no shaders, images, buffers, GPU command buffers or submissions. The parent alone executes it against separately pinned loader, layer and validation dependencies.

The accepted pair uses the existing OFF2 capture and a new ON3 capture, with the same 14 final sources and the same fixture/layer binaries. The four original queries returned 16 opaque bytes in total. OFF2 deliberately retained no hook queries despite its two original calls. ON3 retained both original queries, totaling eight bytes, and joined the original typed arguments, device generations, output pointers and returned spans exactly. Each run saved one scope with ten lifecycle records and ended with zero active instances/devices. The enabled owner join and finite saved-scope check passed; the disabled zero counters make no absence claim.

The parent also checked two literal joins without production-auditor helper functions. The relation between application and layer physical handles remains unknown, as does any mapping between their creation-output slots. No offset or pointer proximity supplies such a mapping. The exact query witnesses validate only the recorded boundary. In this control, samplers belonging to two distinct devices returned identical four-byte payloads. Their opaque bytes serve as per-call integrity evidence, not resource identity or a universal descriptor encoding.

The physical runs performed device/sampler API calls and descriptor queries, with no GPU commands, submissions, shaders, images or buffers. Khronos validation reported zero errors and eight warning callbacks, four per run. No warning-free claim is made. Reported module paths and parent disk pins were checked; loaded process-memory bytes were not independently hashed, and source/binary equivalence is not proven.

A read-only CPU review rehashed 59 files unchanged, reconstructed the actual witnesses and refused 27 in-memory falsifications. The reviewer authored the Python auditor and was independent of the fixture, native transport and physical runner; this is not independent validation of their own parser. No pixel oracle is involved because this probe renders no pixels. The parent alone performed the physical executions. CI is still a separate future check at publication.

## Retained preparation ledger

The first preparation completed only the disabled execution: two original queries returned with zero Khronos errors. Its loader-module attestation was refused because `dladdr` on the imported function address identified the executable's PLT. The fixture's subsequent module-path correction uses an already-loaded library handle and its resolved symbol address. The first capture, source freeze and build remain intact; no enabled execution or combined certificate is derived from it.

OFF2 then returned two queries with a correct loader path. Parent postprocessing initially tried to read that host path inside the sandbox. The correction translated the host path to its readable equivalent for disk-pin verification; it changed no original capture or native observer. OFF2 was recovered by CPU postprocessing and was not repeated. ON3 was the new enabled execution. Their separate source freezes and parent-runner versions are retained; their 14 accepted sources and fixture/layer binaries match. No production, test or auditor source changed after the accepted runs.

Resource generations, selected-slot placement, current descriptor contents at draw, GPU descriptor reads and exclusive phase attribution remain false. Unclaimed processes/namespaces remain unknown. These journals do not repair the earlier translated capture, certify a translated D3D12 query scope or query absence, identify a game sampled image, admit numerical game UV decoding, or constitute a headset/OpenXR test.
