# Original typed-call receipt transport

`native/core/typed_copy_transport.hpp` supplies a bounded, CPU-only metadata
session for explicitly instrumented owned call sites. It does not intercept
Diablo or arbitrary COM calls. The existing pure historical model is unchanged.
No descriptor payload or mapped memory is read.

The caller records original arguments before CreateShaderResourceView,
CopyDescriptorsSimple or CopyDescriptors, invokes the original function with
unchanged operands, then records its actual return. These APIs return void;
the wire retains `result:null`. Metadata disabled, exhausted, malformed or
faulted never suppresses the original call. The original function runs outside
the metadata mutex. The producer adds no GPU resource, wait or submission.

One Session transition counter orders supplied CPU entry/return facts.
Generations are assigned at original creation entry and become usable only
after a successful actual creation return. Release entry stops mutation
admission. Final closure requires the owned final-reference protocol and the
actual original zero reference-count return. A nonfinal return is a gap, not a
claim of destruction. Reset creates a new epoch; stale or duplicate callbacks,
exceptions, reversed returns and overlapping mutations cannot become a
complete historical lineage. The source version remains the one at copy ENTRY.

Four shared live reservations retain immutable receipts. Each saved wire is
at most16KiB, with at most three slots and eight mutation records. Holding a
receipt holds its quota, including copies. No selected receipt is evicted,
looked up from later state, or retagged. These bounds do not describe all CPU
heap allocations. Unsupported facts remain diagnostics or unknown history.

`tools/typed_copy_transport_validate.py` consumes the new wire with the frozen
`typed_descriptor_copy` model. A separate finite journal retains original
call IDs, arguments and entry/return transitions. That journal has its own CPU
clock; explicit references join the two clocks without treating their numbers
as equivalent. The auditor checks successful creations, original heap getters,
binding, command generation and recording epoch, Close, mutation before
Execute, same-queue monotonic producer fences and the actual readback hash.
It never imports observations into an old receipt.

The new two-case owned renderer is
`native/tests/typed_descriptor_copy_fixture.cpp`. It preserves the old renderer
and its HLSL/DXIL. It makes genuine per-case SRVs in one cumulative Session:
three writes before the first capture, seven mutations before the second.
Case1 renders direct A. Case2 copies CPU source B to the selected visible slot
after bind and Close, before Execute. Each receipt is sealed before Execute,
retained through completion and exported after its readback. Capture failure
does not condition rendering or fence handling.

The tests use mock original functions with real C++ signatures, exact void
returns, NULL size arrays, reentry, faults, pending/reversed completion,
generation reuse and quota retention across80copies. The emitted native wire
is actually parsed by the frozen Python model. The standalone query-diagnostic
CMake project registers both CPU controls on Linux and Windows. Windows also
compiles the new PE; CI performs no graphics execution.

Even a positive original-call and pixel join describes this owned experiment.
It does not prove descriptor bytes at copy or draw, unchanged contents,
dynamic shader access, GPU resource identity, renderer ownership, module
backing, a general PE/Unix conversion, API absence, or the game's sampled
input. Those stronger fields remain false. The old query, owner and backing
gates and historical certificates keep their original results.

## Owned execution

The new PE was run serially with transport off and on in a fresh private Wine
prefix under Steam Runtime. The actual wrapper returns were both zero and
were persisted before postprocessing. Four 256×144 BGRA readbacks total 589824
bytes match both the existing Fraction reference and the separately frozen
integer reference exactly. Corresponding off/on images match over 294912 bytes,
counted once. The late B case differs from direct A in 73152 B/A byte positions
per execution.

The ON receipts deliver the actual three- and seven-mutation prefixes, with
their original binding, Close, late-copy, Execute, fence and readback joins.
OFF receipts have no historical snapshot; their missing metadata is not
evidence of absent API calls. Each original journal has 89 calls/178 edges. The
two receipts retain 3954 and 6096 bytes, below the 16KiB cap.

The private checker reports zero non-sentinel validation errors and two
intentional activation sentinels. It also retains 40 Vulkan Loader warnings,
six validation-settings warnings and four runtime OpenXR extension-query
warnings. No XR rendering, headset use or complete validation of dynamic
descriptor accesses is established. Reported module paths and before/after
disk hashes are joined in this new process; loaded-memory backing and a
source/binary equivalence remain unknown. Sources and binaries were frozen
before these runs, and no GPU retry was needed.
