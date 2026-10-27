# Typed-copy source adapter: CPU foundation

The new `native/translator_diagnostic` project is a CPU prototype for observing
original typed descriptor calls at reviewed translator source boundaries. It
does not build vkd3d, patch an installed DLL, load Wine or a graphics driver,
intercept Diablo, or establish a runtime source/binary relationship. The
previous owned renderer and its four readbacks remain separate evidence.

The source contract is pinned to vkd3d-proton commit
`0bb64c56c8f0dcc11ecc0f6c56e00e4f7bd8d506`. The wrapper proposal covers three
CreateShaderResourceView implementations, eight CopyDescriptorsSimple
implementations and the original CopyDescriptors range entry. Explicit target
macros name the original source delegates; no vtable offset is guessed.
Compile-disabled targets preserve the original names. Unsupported or unwrapped
dispatch paths cannot acquire a complete source relation.

## Two boundaries

The C control interface accepts a bounded nonce span, one-shot creation-role
admission and a capture sequence for sealing. It has no app setter for resource
pointers, descriptions, heap spans, descriptor arguments, source versions or
phases. The separate source callbacks receive those values from the original
wrapper arguments and actual returns. Admission labels select the finite owned
domain; they do not attest supplied facts or identify a game's resource.

The C++ implementation reuses the frozen `typed_copy_transport.hpp` through a
new C ABI. The outer receipt has a distinct source-prototype purpose and keeps
the old native wire as a nested mathematical input. The old history model and
old wire auditor are unchanged. A source prototype result never promotes an
old receipt into observed translator runtime evidence.

Metadata locks end before calling any original function. The wrapper preserves
the original parameter pointers, values, nullness, calling convention and
return. Every void descriptor operation retains a null result rather than a
fabricated HRESULT. Disabled, faulted, unsupported or full metadata still
forwards the original exactly once. Bounded typed arrays and description
members may be copied; opaque descriptor bytes and mapped memory are never
read. No observer GPU resource, wait or submission is added.

## Lifetimes and interpretation

The internal device factory may return an existing singleton. Its result is
recorded as an internal factory result, never as the public D3D12CreateDevice
HRESULT. A distinct proposed source-site hook at genuine new initialization
is required to admit a new device occurrence. A known singleton retains its
generation; an unknown singleton cannot manufacture one. The public outer
result remains unknown in this first prototype.

Resource and heap creation arguments are frozen at original entry. A defined
successful interface output is read only after the original return; a failed
or pending output is not dereferenced. The wrapper admits a small declared
description profile and does not certify every unretained creation argument.
CPU and GPU heap getters preserve the source's explicit output-structure
pointer ABI. Their returned values, and the actual descriptor increment,
provide checked unsigned handle arithmetic without dereferencing a handle.
The prototype uses these getter observations locally; it does not yet export
immutable getter ENTRY/RETURN, object-generation, pointer and value-known
occurrence receipts. Those receipts must exist before a future runtime bind
join. A later lookup of the current getter state cannot replace them.

Release entry blocks further mutation admission. Its actual reference-count
return closes the admitted COM occurrence only when the finite policy permits
it. Zero does not prove physical destruction, GPU completion or module backing.
There is no reset API that can recreate still-live source objects in metadata.
Tokens retain their source epoch and occurrence; ambiguous, stale, pending,
reversed or conflicting observations remain unknown.

The source assigns its own bounded CPU windows and API labels. App journal
clocks, query phases and old process identifiers are never imported. The sole
admitted visible slot zero is selected from actual source heap observations;
seal supplies no typed destination or source facts. Receipts retain values,
not a later lookup of a resource or slot.

Four shared tickets per adapter/run retain at most three slot identities and
eight typed mutations each. The complete outer wire remains at most16KiB;
overflow refuses metadata without evicting a held receipt. Copies retain the
same quota pin. These limits do not bound all CPU allocations or all contexts.

## Validation boundary

The isolated C forwarding harness uses fake original delegates and callback
witnesses to compare original operands and returns. The separate integration
harness links the actual C++ adapter, passes the actual wrapper-produced
facts through it and emits receipts consumed by the frozen Python model.
These are distinct controls; linking an unused static library is not an
integration proof. Real SDK declaration compile controls are also distinct
from mock-type execution and from a complete vendor build.

The final local preflight passed 829 Python tests, including 33 new auditor
tests, and all six tests in the separate CMake project. The independent C ABI
suite covers 55 cases, 985 assertions and 685 fake original calls. Its 50 wires
contain 46 snapshots consumed by the unchanged model and 22 known internal
histories. The isolated wrapper suite has 487 assertions; compile-disabled
forwarding has 63. The real C ABI integration has 72 assertions and three
additional model-consumed wires. These controls exercise different boundaries;
their overlapping counts are not a combined coverage total. Local Windows
objects were compiled, not executed. Local sanitizer execution is not claimed.

The public summary is [typed-copy-source-adapter-v1.json](proofs/typed-copy-source-adapter-v1.json).
Private proofs retain the source hashes, separate dependency checkpoints and
authorship limits of the reviews. A reviewer of the parser authored the
wrappers and earlier transport/model; this is not independent certification of
that reviewer's own producer.

`tools/typed_copy_source_validate.py` audits only the new prototype envelope.
`prototype_source_history_known` can describe complete supplied CPU history;
`source_runtime_relation_known` and `public_device_create_result_known` remain
false. Input attestation, descriptor contents at copy or draw, unchanged
contents, shader access, GPU resource identity, renderer ownership, generic
PE/Unix mapping, backing and global API absence remain false as well.

Before any future private vendor build, freeze and review the source adapter,
wrapper insertion points, all selected source/submodule/dependency versions,
build recipe and compatible private DLL pair. A future owned execution must
retain independent original app arguments and same-run seal/bind/Close/
Execute/fence/readback joins. Existing pixels and source files do not attest
that future binary. No new game run follows from this CPU foundation alone.
