# Owned executable and execution-file checks

This component prepares a new, owned two-case application for the private
translator. It does not establish a usable Diablo VR solution. The old
renderers, shader binaries, original game files and supplied-fact auditors are
preserved.

`native/tests/private_translator_runtime_owned.cpp` calls the public C++ COM
interfaces from the complete generated headers. It preloads the selected
private core, resolves its six control exports, and requests the base
`IID_ID3D12Device`. Only nonce, admission roles, sequence numbers, aliases and
buffer capacities are application inputs to the controls. Descriptor facts,
source generations and source clocks are produced inside the translator.
The application does not link another diagnostic context into the executable.

The cases are `direct_A_clamp` and `after_bind_B_clamp`. The latter copies the
original B CPU descriptor to visible slot zero after bind and Close and before
Seal, ReceiptCopy and Execute. Four selected SRV getters are called once and
their actual values and application call references are cached. Other
application calls remain in the original journal. The finite audit projection
is a declared subset; it does not claim to capture every original argument.

The new application voluntarily resets the command list with a null initial
pipeline and then calls SetPipelineState with its actual pipeline. It always
registers and waits for the same producer fence value. These are explicit
application choices shared by its OFF/ON workloads. They do not rewrite the
old renderer's arguments or establish equality with its API schedule.

Metadata capture surrounds each original delegate with separately guarded
BEFORE and AFTER construction. A metadata allocation failure, exhausted
budget or malformed observer result must not suppress the original delegate
or its cleanup. The portable protocol tests exercise that boundary without a
driver. They do not execute the real COM implementations.

`tools/private_translator_runtime_execution.py` reads a fixed, bounded capture
directory and separately pinned ordinary artifacts. It checks exact OUTER
ReceiptCopy bytes and guarded application buffers through the frozen
`private_translator_runtime_validate.analyze`, once per run, preserving that
entire result. It also checks the child journal, public device-create result,
explicit child module paths and control entry-point owners, readback hashes,
and every packed BGRA byte against the pinned integer oracles. File
consistency and the declaration that execution occurred remain distinct.

The reader never launches a process. Its injectable launch sequencing saves
the original wrapper return before postprobes. The private parent runner is
disarmed by default and uses the same sequencing and bounded drain code.
Log retention errors leave the original process running and make log
completeness unknown. A successful file audit is not permission to launch.
Source review, immutable inputs, executable and dependency pins, and the
parent's separate execution evidence are required before a physical control.

The finite file profile allows 32 capture files, 8 MiB per log, 512 KiB per
metadata file and a complete 16 MiB execution report. At most 32 pinned
artifacts are each limited to 64 MiB and together to 512 MiB. The inherited
supplied-fact profile retains its 512 KiB envelope, 256 calls/512 edges, two
16 KiB OUTER payloads, four Copy observations and complete 4 MiB report.
These are protocol and serialization bounds, not a bound on total CPU memory.
The packed 256 by 144 BGRA output contains 147456 bytes; its original footprint
and allocation are checked separately. It is not the old scripted CPU
checksum buffer.

Even execution of this owned application would not establish current
descriptor contents, absence of a silent overwrite, a sampled resource GPU
read, game identity, renderer ownership, backing identity, source/binary
equivalence or global API absence. CPU prototype children keep their original
false runtime/build claims. An actual execution needs a new external proof;
no old certificate is promoted by changing a flag.
