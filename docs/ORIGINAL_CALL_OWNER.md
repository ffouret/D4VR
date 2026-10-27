# Original sampler call owner

This is a separate, opt-in, owned diagnostic. It identifies an explicit original
sampler name/query occurrence across the reviewed Wine x64 boundary. It does not
replace the module/process gate in `translated_owner_witness.py`, repair an old
refusal or authorize rendering. No game integration is added.

## Original calls and activation

`native/tests/original_call_owner.cpp` creates a minimal owned D3D12 device and
uses the reviewed typed `ID3D12DXVKInteropDevice` prefix. It records successful
`GetVulkanHandles`, count-then-values `GetInstanceExtensions`,
`GetDeviceExtensions` and `GetDeviceFeatures` results from that same live object.
Both extension lists are bounded to 256 unique names of fewer than 256 bytes.
The instance list must actually enable `VK_EXT_debug_utils`; the device list and
features must actually enable descriptor buffers. Advertisement, an environment
variable, a non-null procedure or a validation activation message is insufficient.
Unavailable activation causes normal preparation refusal before naming.

The actual sampler descriptor size is queried through the interop physical
device and must be 1..256 bytes. The fixture creates one sampler with a null
allocator, then executes two original pairs:

1. `vkSetDebugUtilsObjectNameEXT` on that sampler with the exact ASCII name
   `D4VR_CALL_OWNER_1:<fresh-32-lowercase-hex-nonce>:1` or `:2`.
2. `vkGetDescriptorEXT` for the same sampler into its own retained output span.

Naming is resolved through GIPA of the interop instance. Each name occupies 53
bytes including NUL, below the 64-byte limit. Names have null pNext and sampler
object type. VkResult is retained for creation/naming; void query/destruction
does not acquire a result code. The sampler is explicitly destroyed after both
queries. The two output spans and their individual returned copies are compared
after both queries and sampler destruction; equal opaque encodings between
queries or different devices are never required.

The fixture saves exactly twelve BEFORE/AFTER edges and their original phase
arguments by value. It publishes each checkpoint under a fresh filename, with a
256 KiB receipt limit. The phase file is a global CPU label, never exclusive
translator or GPU causality. Outputs use a private, fresh C-drive directory with
non-reparse ancestor checks. The fixture creates no explicit command queue,
command buffer, image, buffer, shader, submission or readback. This is not a
claim about internal driver initialization work.

## Separate observer schema

`D4VR_DESCRIPTOR_QUERY_OBSERVER=1` together with
`D4VR_ORIGINAL_CALL_OWNER=1` selects journal schema 3 in the separate explicit
query observer. Default schema 2 and the regular tracing layer remain unchanged.
The observer forwards original arguments and returns without inserting a GPU
operation, wait, resource, submission or application rejection.

The new rows preserve sampler creation/destruction and exact naming inputs in
the existing instance/device transition clock. The narrow instance-create facet
captures debug-utils presence before forwarding, and promotes enabled presence
only after original success. Original physical-device enumeration establishes
the parent relation. Common dispatch keys alone are insufficient.

Sampler/name admission is restricted to the explicit case-8 window. Bootstrap
and other calls are counted separately and do not consume selected sampler
slots. Limits are 32 historical sampler occurrences, at most 16 per device;
32 naming records, at most eight per device; and 96 owner rows. They do not evict
an earlier occurrence. Invalid, unknown, overflow and pending states remain
explicit. Existing eight persistent claims per run, 32 queries per journal,
four per case, 256-byte query output, device-route and lifecycle limits remain
unchanged. These are record limits, not a global CPU memory bound.

## What can be joined

`tools/original_call_owner_validate.py` is a pure, bounded parser and auditor.
It takes the original PE receipt, claim/journal bytes and explicit external pins;
it does not open files, inspect a process, execute a runtime or read mapped
memory. It keeps PID, start cookie, DSO epoch, slot and parent/device/sampler
generations distinct. A positive control requires the exact nonce-bearing
original calls, successful returns, matching original typed sampler queries and
returned bytes, finite complete scopes and explicit target lifetimes.

The pinned generated non-WOW64 Wine thunk copies `pNameInfo` to a Unix temporary.
Its pointer must not be compared across domains. The sampler handle is unwrapped
by the reviewed generic route; the device is converted to its host device. An
explicit marker joins this one occurrence, not a general handle conversion.
PE and Unix extension lists/counts remain separate facts; both must establish
enabled debug-utils, but their counts are not assumed equal.

Even a positive result does **not** attest module backing or loaded memory,
identify a rendering device, establish descriptor placement/current contents or
prove a sampled GPU read. It proves neither global query absence nor the absence
of silent CPU descriptor copies. No old incomplete query log is repaired, no
Diablo input is identified and no coverage or XR result follows from this test.

The private runtime ledger, source/binary pins, exact results and retained
failures accompany the public proof. Mock forwarding and compile-only Windows
checks must remain distinct from an actual Wine/Vulkan execution.

## Retained owned runtime result

The two private Steam Runtime/Wine runs both returned zero. Each original PE
receipt contains twelve edges, two markers and two four-byte sampler returns.
The enabled instance and device lists contain six and 79 names respectively.
The observer-disabled run remains schema 2; its zero captured queries proves no
absence. The enabled schema-3 run joins both original pairs to the same Unix
PID/cookie/DSO epoch and live device/sampler occurrence. Its controlled sampler,
device and parent instance all have explicit successful CPU teardown records.
The PE instance list has six names while the native parent has eight; both
actually enable debug-utils. No identity of those lists is inferred.

The strict result is still `original_call_owner_known=false`: each saved scope
has one additional instance without an observed destruction. These are not the
target parent, and neither has a recorded device in the retained replay, but
their origin or cache behavior is not established. The two exact joins are
retained separately from `all_saved_scopes_complete=false`. No current validator
was relaxed and no extra runtime was launched merely for a final counter.

Across these runs there are zero recorded non-sentinel validation errors, two
activation markers, 40 loader warning lines, six validation-settings warning
lines and four runtime OpenXR extension-query warnings. This was not an XR
rendering/submission test and no pixel was rendered by the fixture.

After OFF, one Python audit predicate and its test were strengthened to reject a
creation explicitly marked lifetime-unknown even when later rows claim known
state. Native code, PE, observer binary and runner did not change. The original
OFF freeze and raw data remain immutable; the additive CPU review and the new
pre-ON freeze distinguish these hashes. OFF was not repeated for that CPU-only
change. See `proofs/original-call-owner-v1.json` for this ledger and limits.
