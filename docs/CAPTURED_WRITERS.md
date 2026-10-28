# Capture-owned prefixes of original source operations

A delivered color image now carries a bounded CPU prefix for its actual original
BLIT source. This closes the earlier gap between the delivered source identity
and the rolling depth journal. The prefix records declarations, commands and
transfers; it does not establish their GPU execution, the last writer, initialized
pixels or a sampled resource path.

## Reservation and ownership

After the real Capture accepts an original BLIT, SourceHistory validates and
admits that exact live source. `reserveCapturedBlit` checks the same observer,
command recording, ordinal and original endpoint lifetimes. Its result is bound
to the accepted capture sequence and original five-label reference. Capture
attaches it once before submission. Explicit unavailable results retain that
binding without changing capture eligibility.

At most four shared payloads remain live. The observer holds weak references;
actual copies, leases and receipts own the payloads. Recycled or discarded copies
release their ownership, while a selected lease cannot be evicted to admit a
new ticket. This quota is independent of the cumulative depth report slots.
Generic captures without consumer observation keep an explicit unbound absence.

## Frozen before the original submission

The first unique original occurrence freezes a source-only prefix in
`SourceHistory.before`, before forwarding the submit. Selection excludes the
original capture BLIT and later commands at its recording/ordinal cutoff.
The body serializes image descriptions, operation values and original application
pipeline metadata once. Later export reads that body, not a live image, camera,
command or pipeline registry.

Incoming COPY/BLIT candidates are distinguished from outgoing source reads.
Draw membership is copied from the actual dynamic-rendering color attachments
and their scope ordinal; nearby depth events do not confer color membership.
The original catalog is separate from a private selected pipeline variant.
Compute, aliasing, descriptor placement and unobserved routes remain unknown.
A first admission cannot recover earlier excluded writers, so
`missing_before_admission` stays true and the admission start is retained.

Original AFTER results are separate from this frozen body. Completion values
are shared with acquire/release synchronization and keep each occurrence's exact
CPU result. Pending results remain VK_NOT_READY. Repeated occurrences mark
ambiguity without replacing the first frozen body or its original identity.
Reset before submission and observer destruction leave explicit unavailable
states. Completion here is an original CPU return, not GPU completion; the
existing producer fence is checked independently by Capture.

## Bounds

Each payload keeps at most 64 events, four CPU occurrences and 128 KiB of JSON.
Its effective event cap is reduced from the remaining configured half-node and
half-reference reservation, accounting for the existing prefix/forward depth
reports and four simultaneous payloads. Runtime configuration reserves 3,072
slots for those reports plus at most 256 for writer prefixes, under 4,096 of
8,192 nodes; the global 32,768-reference cap remains in force. Configurations
without headroom return an unavailable budget result.

Temporary event references obey the shared budgets and are released after the
body is serialized. Selected payloads retain immutable CPU values and at most
four completion references, with no event nodes or snapshot references pinned.
The four serialized bodies add at most 512 KiB of payload storage, excluding
allocator, transport wrappers, recordings and the borrowed catalog. This is not
a total CPU memory cap. No GPU resource, command, wait or submission is added.

## Evidence and next step

The independent `tools/captured_writer_validate.py` first checks the original
application BLIT arguments, exported pixels and saved lifecycle stages. It then
checks the carried prefix, exclusive cutoff, exact counters, explicit source
membership and bounds. Owned WSI controls additionally compare the original
render-scope declarations and shader stage fingerprints against fixture inputs.
Raw Vulkan handles are not joined directly to the observer's catalog labels.

A source prefix is preparation for a restricted causal pixel experiment. It is
not a repair of Diablo's wide-field cutouts, a complete resource graph or an XR
submission. Each real-game trial is recorded separately from the owned controls.


## First actual Diablo source prefixes

The [new game observation](proofs/diablo-captured-writer-prefix-v1.json) delivers
six monoscopic successive pairs and three D24 readouts. All twelve eyes retain
their exact saved lifecycle stages and available prefixes. The first prefix is
empty; the other eleven contain six events each, including an explicit
2560×1440 color-attachment declaration and one draw of the same unmodified
application pipeline. Three alternating source lifetimes belong only to this
observer. All prefixes retain their earlier-history gaps.

This identifies a concrete stage to inspect rather than joining a similarly
sized image from a depth window. It does not identify the sampled input or prove
GPU execution, effective viewport/scissor state or initialized pixels. The
wide-field cutouts and central rectangle remain visible. The game exits through
its normal UI, launcher status is zero, all owned processes exit, and the
original audit including 262 named exports is unchanged.

The [owned controls](proofs/captured-writer-prefix-v1.json) remain separate:
four native/Steam Runtime WSI cases, eight available prefixes, forty retained
events and sixteen independently checked original draw calls. Their original
references compare 67,108,864 bytes across observation off/on, with 4,194,304
exported bytes exact and zero Khronos errors. Eighteen CPU cases cover bounded
retention, concurrent completion reporting, resets, repeated occurrences and
budget failures. Seventeen independent auditor tests reject malformed evidence.
