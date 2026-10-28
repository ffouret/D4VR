# Supplied SOURCE history for the owned first writer

This is a CPU research component. It calculates a historical relation from
supplied original-call facts for the full owned renderer profile. It does not
observe the translator, identify Diablo's input or writer, or provide a usable
VR solution. The last Diablo rectangle and cutouts remain unexplained.

The [precode contract](OWNED_SOURCE_WRITER_CONTRACT.md) and its
[exact grammar](schemas/owned-source-writer-v1.json) were published before this
implementation. They remain unchanged. The older minimal
`source_emitted_writer_validate.py` model also remains unchanged: its one-table
profile cannot admit this renderer's sampler table and PIXEL constants.

## Calculation

`tools/owned_source_writer_validate.py` exposes `analyze(bytes | str) -> dict`.
It performs no file, process, driver, game or runtime observation. The 62 types
and 46 callback projections match the frozen grammar. Malformed input raises
`ValueError`; well-typed missing, contradictory or unsupported evidence returns
a complete unknown report. Every report includes a fresh value copy of the
entire supplied input.

The admitted cold profile contains six resource births, four heaps, eight
original getters, the three original root parameters, two samplers, PIXEL
constant words, and the RTV arguments. The model keeps internal device/root/PSO
associations separate from COM receivers and Vulkan objects. Numeric equality,
pointer proximity, code hashes and labels do not establish those associations.

The command history includes the genuine constructor Reset under its pending
base creator, setup Close/Execute, and the first-case Reset with ordinal 2. The
retained recording epoch is that Reset's actual CallID. The original Draw must
enclose the actual native Emit, bind and WithCount raster observations. Close
must return successfully before Execute ENTRY, and Execute RETURN must be
present before the seal.

For the mutable SRV table, each copy freezes its source history at that copy's
ENTRY. The visible destination history is selected at Execute ENTRY. A later
mutation cannot replace either cut through a lookup at seal. Additional cold
mathematical copy cases test this rule; they are not captures of a changed
physical workload or of the warm second case.

Only the declared monolithic native route can produce a known relation. It
requires genuine module, graphics-layout and pipeline births, immutable PSO/root
associations, and original lookup/bind/emission arguments. Modules must be live
through PipelineCreate; they may be destroyed after it returns. Unsupported
GPL, descriptor-heap, cached-without-origin, asynchronous, internal-copy and
other routes stay unknown. The branch used by a future real writer remains to
be observed.

An original void return is JSON null, with a separate returned transition. It
is not a synthetic HRESULT or a pending call. Failed creators cannot publish
new after-associations. COM Release applies to the eight COM kinds in the
contract; native objects use their actual destruction callbacks. A nonfinal
Release does not free an identity for reuse or prove GPU destruction.

## Meaning of a known result

`historical_declared_bound_view_known=true` means the supplied history satisfies
this finite contract. The result names its original view call, resource,
declared mip extent, copy lineage, root slot and cut. A declared mip extent is
not an effective LOD, numerical UV, sampled input or initialized-pixel claim.

All seventeen stronger claims remain false, including input attestation,
runtime coverage, actual sampled-input identity, current descriptor/texel
contents, sampler identity, GPU read, native image binding, renderer owner,
backing, loaded binary/source equivalence, game identity and VR success.
Silent-overwrite twins can therefore have identical reported histories.

## Limits

Each supplied frame has strict limits: 64 KiB UTF-8, 4096 value/container nodes,
depth 20, strings of 256 bytes, 64 call records, 128 retained transitions or
observer marks, and 32 pending calls. One ticket and one first-case recording
epoch are retained. A complete report is limited to 256 KiB. Duplicate keys,
booleans in integer fields, floats, invalid Unicode and arithmetic overflow are
rejected or classified according to the frozen contract. Input and output are
never truncated to fit.

These are frame and serialization limits, not a bound on total CPU/process
memory, an input-coverage guarantee or a proof that every real route fits.
Fresh-report churn checks immutability; this pure function has no hidden global
ticket registry.

## CPU mock harness and validation

The new `native/tests/owned_source_writer_fixture.cpp` uses typed mock arguments
and returns before projecting JSON. HRESULT, VkResult, UINT, pointer,
VkPipeline and void returns remain distinct. Metadata guards surround the
BEFORE and AFTER builders, and invoke each delegate exactly once. Caller
allocation failures and exceptions inside a delegate are outside that guard's
guarantee. No metadata mutex surrounds a delegate.

The harness runs the same mock workload in on, off and faulted modes, including
cleanup, and compares its same-process original argument/return traces. It
emits six frames: cold first writer and warm continuation in each mode. The
cold on frame can be known. The warm continuation reuses origins, performs
Reset 3, exceeds the finite capture and stays unknown; the earlier cold frame
remains immutable. Its clock 128 is the last allocated observer mark, not all
170 transitions of its 85 observed mock originals. The original seen/returned
counters and drop/fault/overflow flags remain separate. Off has no retained
rows and proves no absence of calls.

Separate guard cases exercise BEFORE/AFTER exceptions and 33 nested mock
delegates against the 32-pending limit. The workload has 303 delegates across
three modes, including 48 cleanup delegates; 76 additional guard delegates are
a different count. Assertions and author/independent checks overlap and must
not be added as independent evidence.

`tools/owned_source_writer_native_wire.py` sends each of the six actual emitted
wire texts to the model exactly once and retains each complete result. This is
a CPU mock integration, not a vendor ABI test, SOURCE producer or GPU readback.

The independent Python suite covers exact full-report output, the real owned
profile, causal copy cuts, cold mathematical late-copy and lazy-birth cases,
pending/reverse/reentry, releases and reuse, missing scopes, raster, mips,
overflow, malformed framing, quotas, fresh-report churn and overwrite twins.
Only new local controls are required for this component:

```sh
python -m unittest discover -s tests -p test_owned_source_writer_validate.py -v
cmake -S native/owned_source_writer_diagnostic -B work/owned-writer-cpu
cmake --build work/owned-writer-cpu
ctest --test-dir work/owned-writer-cpu --output-on-failure
```

CI runs these new CPU controls on Linux and Windows and builds the new C++
mock with Linux ASan/UBSan. Python is not sanitized. Neither the local mock nor
CI runs a new vendor DLL, owned renderer PE, Wine, GPU, Diablo or headset.

The next useful step is a separately reviewed C-only SOURCE sink and admission
route that captures the real view/bind/writer originals. This model does not
authorize that producer, a vendor rebuild or another physical run. The
[implementation proof](proofs/owned-source-writer-v1.json) records this CPU
milestone separately from earlier physical translator receipts.
