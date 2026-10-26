# Observing depth access before adding a read

The actual Diablo audit rules out enabling the current [attachment reader](DEPTH_READBACK.md) unchanged. The game uses original semaphore scopes, command flags and image/view descriptions outside that reader's deliberately narrow policy. These are policy mismatches, **not evidence of invalid Vulkan use**. No depth pixels were read and the wide-field coverage problem remains unresolved.

`depth::Audit` is now instantiated by the regular layer with `D4VR_DEPTH_AUDIT=1`. It observes CPU arguments and original results only: no GPU commands, allocations, waits, mask changes or application-call rejection. This option takes precedence over `D4VR_DEPTH_READBACK_DIAGNOSTIC`, even if both are set. Other injection modes remain separate options; use an isolated audit configuration and check `other_injection_modes_configured=false` and `frame_trace_enabled=false` in its report.

## Reports and provenance

Set a private `D4VR_TRACE_DIR`. Reports are written after original presentation at powers-of-two boundaries, on a higher decimal counter in the regular file `depth-audit.request`, and before device destruction. At most 32 immutable reports per device are attempted. The counter file is bounded, opened without following symlinks and checked as a regular file; a FIFO cannot block the polling path. Reports do not depend on reaching device destruction. File output is synchronous CPU work, not a real-time cadence guarantee.

Each report preserves:

- Registered queue families and indices, submit API/count/shape, original wait/signal masks, device indices/masks, first extension type and original results.
- Image flags, format, extent, usage, sharing, samples/mips/layers; view ranges/swizzles and the presence/first type of extension chains.
- Command recording flags, level, pool family/flags and extension presence; dynamic-rendering area, layout, load/store/resolve and clear-depth bits.
- Image/view and recording generations, rendering ordinal, recorded end counts and successful CPU submit observations, with eligibility from the reader's shared pure predicates.

Descriptions are grouped by values, including clear bits, rather than by resource handle. A submission snapshots its command observations by value **before** the original call. A later reset, handle reuse or late callback cannot replace those observations with a new generation. A successful return establishes a successful API call, not completed GPU execution or initialized depth. The report's `last_submitted` is the last callback observation, not necessarily the greatest submission sequence under concurrent calls.

The observer has no Vulkan dispatch table or GPU resources. Its CPU mutex is released before forwarding. Exceptions latch a report fault and cannot reject the original application call. Registries are capped at 64 queues, 65,536 images and views each, 32,768 commands, 128 rendering descriptions and 32 entries per histogram. Submission inspection is capped at 256 batches/commands; report payloads are capped at 512 KiB. Overflow, truncation, unknowns and faults remain explicit. Secondary executions are counted, not expanded. Only the first extension type is sampled: this is **not** a full extension-chain interpretation.

`scope_contract_observed_compatible` describes the inspected public submissions against the reader's policy. Even a true value would not certify future calls, hidden/downstream dispatch, external producers, ownership, initialized pixels or a scene. Local counters and presentation boundaries are CPU observations, not frame timestamps or XR provenance.

## Actual Diablo result

[diablo-depth-audit-v1.json](proofs/diablo-depth-audit-v1.json) records a completed original-camera session, without shader overrides, camera updates, depth readback or XR. The selected character entered and stayed still; exit used the normal UI. The launcher returned zero, all owned processes exited, and originals plus 262 named exports matched before/after. The world looked normal; exact pixel equality and in-game Khronos validation are not claimed.

Two on-request world reports retain increasing counters and unchanged description identities. The later report observes 400,573 successful Submit2 calls, including 377,890 outside the current reader policy, with no failed original calls. Thirteen queues were registered across families 0, 1, 2 and 5. All 35 depth-rendering descriptions fail reader admission. No registry/description/histogram/input overflow, unknown queue or unknown depth view was recorded.

The concrete differences include:

- Wait scopes include `BLIT | COLOR_ATTACHMENT_OUTPUT`; signals include `NONE`, `COPY` and `ALL_COMMANDS`. Submit2 stage masks constrain synchronization scope; adding a new transfer read after arbitrary rendering would require a separate proof. See [semaphore scopes](https://docs.vulkan.org/refpages/latest/refpages/source/VkSemaphoreSubmitInfo.html).
- Depth command recordings use flags zero, so the one-time submission guarantee required by the current reader is absent. **This audit does not prove an actual repeated submission.**
- Image flags include `0x00400000`, named `VK_IMAGE_CREATE_ALIAS_SINGLE_LAYER_DESCRIPTOR_BIT_KHR` by the current [Khronos flag reference](https://docs.vulkan.org/refpages/latest/refpages/source/VkImageCreateFlagBits.html), sometimes combined with cube compatibility. The older local SDK does not name that new bit; removing or ignoring it would not establish safe admission.
- View chains begin with `VkImageViewUsageCreateInfo`. Its effective-usage contents were not sampled; the [usage override](https://docs.vulkan.org/refpages/latest/refpages/source/VkImageViewUsageCreateInfo.html) cannot be inferred from the image usage alone.
- Command device masks are zero. Vulkan defines zero as all valid device-group bits, so this is not an API error; the reader currently accepts only its narrower mask-one case. See [command device masks](https://docs.vulkan.org/refpages/latest/refpages/source/VkCommandBufferSubmitInfo.html).

These observations do not identify the scene-depth pass or show whether the missing wide-field color regions contain geometry. The last geometric game experiment remains the held-view result, with its persistent missing regions. No actual game depth data or image has reached XR or the Quest.

## Verification and next access route

[depth-audit-v1.json](proofs/depth-audit-v1.json) separates owned-scene verification from that actual game report. Four real X11/WSI runs compare audit enabled/disabled in native and Steam Runtime environments. All preserve 65 original submissions, 64 presents and 128 draws. An original valid early timeline signal completes with its exact stage mask, including with the conflicting reader option enabled: observation does not become enforcement. Two on-request reports arrive before teardown. The enabled runs preserve 2,097,152 original color bytes exactly, insert zero depth reads and produce zero Khronos synchronization errors.

CPU controls cover immutable generations under reset/reuse, failed original calls, unknowns, bounds, faults and 2,000 calls from two CPU threads. Original forwarding tests pass. Since reader eligibility was extracted into shared predicates, the 24 direct/loader GPU cases and four WSI readback regressions were renewed on the same layer, with zero synchronization errors. These controlled applications do not certify an arbitrary asynchronous game engine. The portable test `depth_audit_observation` is included in Linux/Windows and Linux ASan/UBSan checks; 201 Python tests pass.

The next diagnostic must observe **original depth transfer reads**, their source footprint/layout and command reuse. A bounded duplicate read placed beside an already valid COPY/BLIT access may preserve an existing synchronization scope; that route is **not implemented or admitted yet**. It still needs source initialization, alias/extension and reusable-command lifetime proofs. Do not enable the strict EndRendering reader, broaden application masks, insert global waits, or repeat this unchanged audit. No XR worker or runtime call was added.
