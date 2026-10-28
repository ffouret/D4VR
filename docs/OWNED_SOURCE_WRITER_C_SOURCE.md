# C SOURCE writer sink: CPU implementation

This implements the C-only sink and lease controls specified by [the frozen precode contract](OWNED_SOURCE_WRITER_C_ABI_CONTRACT.md). It accepts bounded, owned POD projections from callbacks. The current integrations exercise original **CPU mocks**; the vendor SOURCE callsites, shader parser bridge and runtime loading are a later step.

The frozen calculation remains `tools/owned_source_writer_validate.py`. A copied frame is supplied to it once, with the complete result retained. Its positive result describes historical declared bindings in supplied callbacks. All seventeen stronger claims remain false, including actual sampled input, current contents, sampler, GPU read, native binding, renderer ownership, backing, game UV and VR.

## Implemented boundary

`native/owned_source_writer_source/owned_source_writer_source_v1.h` contains the exact C PODs and private callback declarations. The generator derives the layouts, metadata and callback entry points from the published normative manifest and annexes. The sink owns context IDs, call IDs, clocks, generations, pending registrations and the selected epoch. A callback cannot supply those values through an `ObjectArg`.

`d4vr_osw_enter_v1` admits a source site before typed argument reads. Each accepted BEFORE projection is copied by value. Observed zero, observed NULL and an unobserved operand have distinct masks. Required unread arguments, reserved flags, incompatible receivers and nonapplicable AFTER fields are refused. Defined nonvoid outputs require a fresh `return_admit` after the original return. Release takes the original ULONG; void callbacks take no fabricated HRESULT. Constructor notes retain the original internal/interface associations.

The first factory candidate and first Draw are retained before their branch is known. The constructor Reset is ordinal 1, and the first case Reset is ordinal 2 with its actual callback CallID. Copy-source history is evaluated at COPY ENTRY and visible descriptor history at Execute ENTRY. The source frame closes after the selected original Execute RETURN. Seal only acquires a lease; it never fills missing origins or reads current state. Warm Reset 3 produces no replacement frame.

The C functions never invoke an original delegate. The separate mock wrappers execute it once between metadata calls, including bypassed, enabled and faulted observations. No metadata lock spans that delegate. Projection safety still depends on the exact source-owned read preconditions; a non-NULL pointer alone does not establish readable storage.

## Retention and bounds

There is one production context, one 65536-byte payload, a separate 65536-byte serialization workspace and eight alias owners. Four public lease operations copy, retain or drop that payload, or acquire it through Seal; Begin and Status complete the six controls. A retained alias has a distinct checked ID. Scalar token copies do not retain ownership. Detached byte copies are outside the payload retention quota.

The compiled C layouts allocate 404352 bytes for the context plus a 64-byte loaded-core issuer. The CPU-only test library has two arenas sharing that issuer, or 808768 bytes. Production has an 8-byte source thread cookie per thread; the test selector adds four bytes. These figures exclude constant tables, code, stack, caller objects, mock traces, detached reports and language/runtime storage. They are not a total CPU memory limit.

The unchanged frame limits are 65536 UTF-8 bytes, 4096 value/container nodes, depth 20, strings 256 bytes, 64 records, 128 marks, 32 pending callbacks and 21 object occurrences. A complete calculation report is at most 262144 bytes. Quota or unsafe projection closes admission without inventing original returns. Registered callbacks can drain after that boundary. A published payload stays immutable after later faults. A serializer losing its final trylock latches failure; it does not silently remain a healthy pending publication.

Spans and overlaps are checked before output writes. A valid alias with a short capacity yields SMALL and the true required length. With sufficient capacity and NULL destination it yields INVALID, written zero and that same required length. Successful Copy writes exactly the payload bytes, preserving guards and the unused tail. Control validation holds the context lock before consulting aliases or allocating an issuer ID; an exhausted owner quota consumes no new ID.

## CPU checks

Use a 64-bit GCC or Clang **GNU driver**. MSVC and clang-cl are explicitly outside this new module's compiler profile.

The lock-free latch requirement uses a C11 `_Generic` selection of the compiler's constant capability macro for the actual `uint32_t` type and requires value 2 (always lock-free). An unsupported type or capability is a compile error. Strict `-pedantic -Werror` remains enabled; no builtin function call is assumed to be an ISO C integer constant expression.

```sh
cmake -S native/owned_source_writer_source -B ../work/owned-C-source-cpu -DOSW_BUILD_CPU_TEST_SUPPORT=ON
cmake --build ../work/owned-C-source-cpu
ctest --test-dir ../work/owned-C-source-cpu --output-on-failure
```

The new CTest group covers independent C causals, typed mock forwarding, a separately compiled OFF executable with no sink linked, and two explicit bounded wire bridges. ABI object probes compile the C/C++ layouts. Additional local compile-only probes use the pinned complete Device15/Resource2/Vulkan headers; the portable CI probes do not contain those vendor declarations.

The forwarding fixture retains six resources, four heaps, eight getters, three root parameters including the SAMPLER table and PIXEL constants, RTV and the actual WithCount argument shape. Its cold enabled frame is a supplied-history positive; warm callbacks and cleanup continue without retagging it. Independent primaries exercise direct A, a copy source frozen at ENTRY, a pending cutoff and a first Draw without an emitter. CPU buffers and checksums are not GPU readbacks.

The bridges accept at most eight real raw files of 65536 bytes each, 8192 bytes of child metrics, 65536 bytes of stderr and a 30-second timeout per owned CPU child. Their complete aggregate limit is 4194304 bytes. They validate all raw lengths and byte hashes before invoking the calculation, preserve each entire result and persist the actual child return before file probes. They use create-only outputs and regular-file identity checks. POSIX opens use nofollow/nonblocking flags when available. Windows uses `CreateFileW` with `OPEN_REPARSE_POINT`, `CREATE_NEW` for outputs and a held read handle without write/delete sharing for inputs. Full 128-bit file IDs, volume, size and basic metadata are queried through the same Win32 API family; unsupported queries refuse the input. Read access time is excluded because reading can update it. Leaf reparse points, directories and non-disk handles are refused before payload IO. The caller canonicalizes the parent directory; this does not establish a nofollow guarantee for all ancestors. These are serialization and protocol bounds, not universal filesystem latency or whole-process memory guarantees.

## Next SOURCE gate

The code has not been inserted into vendor callsites or linked into a newly instrumented translator. Actual SOURCE wrappers, the generic RootParse cut before conversion/free, a stateless shader bridge into one core, cold-origin selection/budget and real linkage remain to be reviewed separately. The proposed default-OFF Meson option and exclusion from the older observer remain part of that future integration. The existing DLL pairs and old six-control observer are unchanged. This CPU milestone does not establish a usable Diablo VR solution.
