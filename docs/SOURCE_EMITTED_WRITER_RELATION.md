# Declared view of a supplied SOURCE emitted writer

Diablo VR remains research and prototyping. The private translator has run in
the owned laboratory, but the game input resource and the cause of its rectangle
and cutouts remain unknown. This next component prepares a finite source callback
protocol; it does not identify a writer in the game.

The exact precode contract is
[`schemas/source-emitted-writer-v1.json`](schemas/source-emitted-writer-v1.json).
It is a closed type grammar, not standard JSON Schema. Its keys, scalar domains,
array prefixes, pointers and limits were frozen before implementation. Changes
need an explicit additive contract revision and evidence; earlier candidates
remain historical checkpoints.

The admitted positive profile is an original COM DrawInstanced with an actual
nested vkCmdDraw occurrence. A returned COM call alone cannot establish that an
emission occurred. Genuine object creation, pipeline lookup and creation inputs,
the command recording epoch, root layout, original setters, Close and Execute
must join through explicit original occurrences. Internal pointers, COM interface
pointers, Vulkan handles, generations and counters keep separate domains.

For that profile, the logical table recorded by the draw is mutable under the
retained original root layout. Its declared typed history is evaluated at the
original Execute ENTRY. A returned public copy of B after bind and Close can
therefore select B. Each copy freezes its source version at its own ENTRY.
Pending or overlapping dependent callbacks, reversed dependent returns,
ambiguous or missing relevant evidence yield unknown.
The result contains the original CreateSRV and copy lineage, not a fabricated
native descriptor or VkImageView. An absent native query does not require a new
query or a different renderer dispatch.

The original root version matters: version 1.0 has absent range flags and the
pinned parser derives flags 3; version 1.1 must retain the original flags 3 for
this mutable profile. Reassigning the same root preserves the table, whereas a
real change or ClearState invalidates it until a complete rebind. The actual
parameter index is retained; no shader register or root index is guessed.

Internal CopyRenderPass, resolve and deferred copies are diagnostic unknown in
this first version. The earlier proposed positive internal A example was
superseded before code: in the retained DXGI28, single-sample, equal-color-aspect
domain, the pinned source takes its image-copy route. A future internal positive
needs a reachable source profile and an original batch-item association. A later
public copy of B must never retag an internal recorded A as B.

The new declared extent domain is Texture2D, DXGI28, one layer and sample,
dimensions 1 through 16384, and legal mip counts up to 15. A single-mip view of
1024 by 512 at mip 2 declares 256 by 128. This says nothing about the effective
sampled LOD, coordinates, coverage or texels. The older 8 by 8 descriptor model
remains unchanged.

One supplied context retains one ticket and command epoch, at most two resources,
two heaps, three descriptor slots, eight getters, eight mutations and 32 pending
callbacks. The input permits 64 records, 128 edges or observer marks, 64 KiB UTF-8,
4096 nodes, depth 20 and strings of 256 UTF-8 bytes. Each original array also has
its own count, pointer, complete flag and prefix cap. The complete output is
limited to 256 KiB, without truncation. These are finite protocol and
serialization limits, not a bound on total CPU memory.

`tools/source_emitted_writer_validate.py` exposes `analyze(raw: bytes | str)`.
Malformed framing, duplicate keys, scalar widths and array-prefix violations
raise ValueError. Well-typed unsupported or contradictory histories return a
complete unknown result. Every report retains a detached copy of the supplied
input. Repeating a calculation does not introduce a global ticket registry.

The new portable CMake project is `native/source_emitted_writer_diagnostic`.
Its C++ fixture emits actual JSON bytes from two separate mock captures: direct
A and a late CopyDescriptors ranges call selecting B, with NULL size arrays.
The captures have 47 and 48 retained calls, and three then four mutations;
these are not the earlier physical run's cumulative three/seven prefixes.
The Python CTest bridge consumes six exact wire texts across on/off/faulted
modes. The two on histories select A/mip2/256 by 128 and B/mip1/512 by 256;
the other four remain unknown.

Mock delegates return a tagged POD value before JSON construction. Void stays
null; signed HRESULT, VkResult, source status, UINT32, pointer, VkPipeline and
Bool domains remain separate. Injected exceptions before and after the mock
delegate leave that return and a subsequent cleanup unchanged. No metadata
mutex surrounds a delegate. These tests cover the explicit observation boundary,
not caller argument allocation or arbitrary exceptions in the original code.

The fixture uses owned storage, closures and projected argument traces. It does
not reproduce the original vendor C prototypes, generate real SOURCE object
identities or implement a production capture/transport service. Raw trace
equality is checked within one process using the same owned storage. The separate
cross-process semantic digest replaces only explicitly named owned pointer roles;
it is not raw wire identity or an opaque pointer mapping. A future real source
adapter still needs its own callback, ABI, selection and transport review.

A supplied history cannot attest its inputs or establish the shader's actual
binding use, current descriptor contents, unchanged texels, a GPU read, renderer
ownership, backing identity, game identity or a VR solution. All stronger claims
remain false. No vendor build, Wine, GPU, game or headset run follows merely from
a positive CPU calculation.
