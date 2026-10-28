# Exact outer receipt bytes and original application arguments

`tools/private_translator_runtime_validate.py` implements a pure CPU calculation
over supplied facts. Its positive result,
`supplied_outer_copy_and_APP_argument_relation_known`, relates two exact outer
receipt byte spans to the declared original application and control calls.
It does not attest those inputs or establish execution of a translator DLL.

The private instrumented translator has been built separately. The fixtures in
this change execute the real private control exports, source wrappers and CPU
calculators with **mocked D3D12 delegates**. They neither load those DLLs nor use
Wine, Vulkan, a GPU, Diablo or a headset. There is still no working Diablo VR
solution. The next decisive experiment needs a new owned executable and runner
that obtain these bytes from the actual translator during the two render cases.

## Byte and ownership contract

`analyze(bytes | str | dict)` accepts the new
`supplied_private_outer_control_APP_inputs` schema. It extracts each original
ReceiptCopy payload from the caller's owned AFTER buffer, checks the original
status, written/required values, capacity, pointers and 16-byte guards, and
requires exact UTF-8 text and SHA equality. Whitespace is significant. It calls
`audit_private_source_wire` once per primary payload and preserves the full
returned calculation. A repeated Copy uses the same retained payload; it does
not add an audit or manufacture ownership.

There is no separately exported raw child JSON span. The auditor does not
reserialize the nested getter object and call that a raw digest. It does not
invoke the earlier bind auditor with reconstructed bytes.

Begin, Expect, Seal, Copy, Retain and Drop use their actual unsigned control
statuses and outputs. A distinct alias returned by Retain owns the same pin;
Drop consumes only that alias. A copied scalar token does not retain anything.
Compiled-OFF inputs have no control binding, calls or receipts. Instrumented
runtime-disabled inputs are a separate negative. A disabled source can return
OK from Seal and Copy for a diagnostic payload whose snapshot is null; those
real statuses remain intact and the argument relation remains unknown.
A NULL Copy output has no
invented buffer: INVALID may preserve a nonzero required size, and SMALL is
checked before a NULL output when capacity is insufficient.

## Original argument relation

Application and source clocks, ordinals and generations are independent.
Explicit unique original occurrences join creation descriptions, parents,
getter results and typed arguments. No equality of generation numbers, clock
proximity, opaque values or hidden struct-return pointers supplies that join.
The four selected getter originals execute once; both cases use their cached
values and CallRefs. All selected objects must remain eligible through the
receipt cutoff. Even a nonfinal Release removes conservative eligibility at
ENTRY; its ULONG result is never promoted to proof of GPU destruction.

The two cases preserve cumulative histories of three and seven typed mutations:
`direct_A_clamp`, followed by `after_bind_B_clamp`. The second copies CPU B to
visible slot zero after the last bind, root table and Close, and before Seal,
ReceiptCopy and Execute. Command-list lifetime, recording epoch, root parameter
zero, queue, fence, completion, Map and Unmap must agree with the original calls.
Void originals retain a null result; control statuses and HRESULTs are separate.

GetCopyableFootprints remains an original void call with separately captured
caller outputs. The supplied 256 by 144 BGRA profile has 147456 packed bytes;
checked row pitch, useful span, total size, allocation and Map range remain
distinct. The pure auditor does not open a readback file, recompute its digest,
verify pixels or infer a GPU read. The CPU fixture fills owned CPU storage and
hashes it; those bytes are synthetic metadata.

## Bounds and refusals

The complete input is at most 512 KiB UTF-8, 16384 nodes, depth 18, 256 calls and
512 edges. There are two cases, at most two primary outer spans of 16 KiB each,
four Copy observations, and a complete compact ASCII JSON report of at most
4 MiB. Reports are refused rather than truncated. These are serialization and
protocol bounds, not a bound on total CPU memory.

Malformed types, booleans used as integers, overflow, duplicate keys, byte or
digest mismatches, alias misuse and contradictory lifetime/order facts raise
ValueError. Well-framed unavailable, failed, pending or faulted observations
produce an unknown relation. Analysis never filters an application call.

All stronger claims remain false, including attested inputs, source runtime,
loaded binary/source equivalence, current descriptor contents, absence of silent
overwrites, GPU reads, renderer ownership, backing and game input. The complete
child calculations retain their original prototype/build/runtime flags. A new
external execution proof must establish future runtime facts separately.

## CPU checks

The new CMake project is `native/translator_runtime_diagnostic`. Its normal,
faulted and compiled-off fixtures preserve the same original mock workload and
perform cleanup before metadata assertions. The wire test consumes the emitted
bytes in the new auditor. Portable test samples are byte-exact outputs from
these CPU processes, including their original supplied pointer values.

Independent causal tests cover exact bytes, guarded buffers, aliases, NULL
outputs, overflow, original lifetime and recording order, source joins, queue
and readback metadata, and silent overwrite twins. Compiler-off is a CPU
fixture profile; it is not evidence of an actual loaded DLL's exports. Linux
sanitizers cover the C++ fixture and calculators when available, not Python.
