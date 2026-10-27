# Owned SRGB output and BLIT control

This new control separates an attachment view's color conversion from the conversion performed by a BLIT using its source image's creation format. It addresses the format gap observed in the [fixed monoscopic UV trial](SAMPLED_UV_DIAGNOSTIC.md). That trial retained original SRGB pipeline declarations and UNORM image/capture declarations; it did not observe the bound attachment view. This control establishes no game binding or numerical game UV interpretation.

The native and Steam Runtime executions both passed on the first attempt, with 144 readouts and zero Khronos validation errors. No code repair, policy widening or GPU retry was needed. The fixture also compiled with strict Linux warnings and as a Windows object. The older UV controls, admission, shaders and certificates remain unchanged; this control has its own [physical and CPU proof](proofs/sampled-srgb-control-v1.json).

## Three paths, each with an original and diagnostic fragment

`native/tests/sampled_srgb_gpu.cpp` reuses the frozen owned-resource helper under its `NO_MAIN` guard. The caller supplies the same unchanged private vertex module, original fragment module and previously constructed UV candidate. All three module byte arrays are retained before Vulkan setup. The candidate preserves the actual sample coordinates, indices, reads and B/A stores; only its previously admitted R/G store values differ. This fixture constructs no shader or game bundle.

| Group prefix | Source image creation format | Attachment view and pipeline color format | BLIT destination format | Conversion being tested |
| --- | --- | --- | --- | --- |
| `unorm` | 44, `B8G8R8A8_UNORM` | 44, UNORM | 44, UNORM | UNORM reference |
| `mutable-srgb` | 44, UNORM, mutable with explicit view list `[44,50]` | 50, `B8G8R8A8_SRGB` | 44, UNORM | SRGB attachment output retained in a UNORM image's raw bytes |
| `srgb` | 50, SRGB | 50, SRGB | 44, UNORM | SRGB attachment output followed by source-image SRGB decoding during BLIT |

Each prefix has `-original` and `-diagnostic` groups, giving six groups. [Attachment output conversion](https://docs.vulkan.org/spec/latest/chapters/framebuffer.html#framebuffer-blending) encodes RGB for an SRGB attachment; alpha has no SRGB transfer. [BLIT conversion](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBlitImage.html) instead uses the image formats: an SRGB source is converted to linear RGB before filtering. A view does not change an image's creation format. The mutable and native SRGB paths therefore distinguish output encoding from subsequent BLIT decoding. No destination is SRGB.

Before creating any image or pipeline, the fixture queries optimal-format features and five exact image configurations: ordinary UNORM source, mutable UNORM source with `[44,50]`, native SRGB source, separate UNORM transfer destination, and the 8×8 sampled textures. It requires successful results, suitable extents, one sample, and the required attachment, sampling, transfer and BLIT features. The [format list](https://docs.vulkan.org/refpages/latest/refpages/source/VkImageFormatListCreateInfo.html) is supplied to both the mutable image query and creation. Unsupported preparation fails; no fallback silently changes a path. The instance and device require Vulkan 1.3; image-format-list support is core in Vulkan 1.2.

## Six fixed cases and two readouts per draw

Every group uses two known 8×8 UNORM textures, two nearest samplers with clamp/repeat addressing, explicit descriptor sets, a 1,280-byte UBO and 28-byte push constants. The six cases are fixed before execution:

| Case | Purpose |
| --- | --- |
| `A_clamp_zero` | Texture A, clamp, zero coordinate offset |
| `B_repeat_offset` | Texture B, repeat, offset by one texel horizontally and minus one vertically |
| `B_clamp_offset_center_scissor_LOAD` | Nonzero LOAD outside a central scissor |
| `B_clamp_offset_center_viewport_LOAD` | Nonzero LOAD outside a central viewport |
| `negative_height` | Signed viewport transformation with the original sampling operation |
| `B_rectangle_input` | A rectangle in the sampled input's B channel, with full viewport/scissor and variable alpha |

The first five cases retain the original owned texture formulas. The last replaces only texture B through an explicit upload: B has a known central 4×4 rectangle and a nonzero exterior, while alpha varies within the texture. The diagnostic's R/G encode the unchanged coordinates independently of those sampled B/A values. This distinguishes an input rectangle from rectangles created by viewport/scissor restrictions; a regular R/G pattern does not imply uniform sampled B or GPU coverage.

Before each draw, a buffer copy initializes the complete source with exact nonzero BGRA bytes `[7,5,3,255]`. The same raw initialization applies to every image/view path. Rendering uses `LOAD`/`STORE`, with blending and logic operations disabled, all four color channels writable, and one sample. No interpretation of an SRGB floating-point clear is required. Outside the draw, native SRGB source bytes must still be distinguished from their decoded UNORM BLIT result.

After rendering, the fixture copies the source directly to a buffer **before** a full-size nearest BLIT into a separate UNORM image. It then copies that destination to another buffer. Each case exports separate `.source.bgra` and `.blit.bgra` files, hashes, exact push/UBO snapshots, viewport/scissor operands, texture variant and BLIT region/layout/filter metadata. Source creation format, attachment view format, pipeline color format and destination format remain separate manifest fields.

The execution recorded 36 draws and 36 BLITs per environment, producing 72 readouts at 2560×1440. Each group has eight serial, fenced submissions: initial texture upload, six draws, and the rectangle-texture upload. Host-coherent buffers have explicit host/transfer/uniform barriers; attachment writes become transfer reads, and readback writes become host reads. The manifest snapshots owned state before teardown and reports validation counters including teardown. A successful fixture result requires zero validation errors.

## Independent finite-case acceptance policy

`tools/sampled_srgb_validate.py` independently derives pixel-center coordinates from signed viewport geometry, nearest texture selection, the diagnostic encoding and the [SRGB transfer formula](https://registry.khronos.org/DataFormat/specs/1.4/dataformat.1.4.html). It distinguishes the source raw bytes from the BLIT destination, including SRGB decoding of unchanged LOAD pixels. RGB transfer is never applied to alpha.

The predeclared policy uses a 0.005-code linear shader budget, a relative `1e-5` budget for each modeled transfer arithmetic operation, outward CPU rounding, and floor-or-ceil quantization after clamping. The neighboring-integer conversion rule comes from [Vulkan fixed-point conversion](https://docs.vulkan.org/spec/latest/chapters/fundamentals.html#fundamentals-fixedconv). The chosen arithmetic budgets are a finite-case acceptance policy, not a normative upper bound on SRGB implementation precision or a universal interpolation guarantee. NaN and infinity are outside this control. A failure must remain a failure rather than widening the policy after observing pixels.

The audit requires exact original/diagnostic B/A byte equality separately at the source and BLIT destination of each format path. It also checks original sampled colors, raw LOAD preservation, the rectangle input, variable alpha and the signed viewport. Alternative no-transfer, extra-transfer, swapped-component and source-versus-destination interpretations must be distinguishable; preserving B/A dependencies alone cannot prove that discrimination.

## Completed owned results

The two environments produced 2,123,366,400 raw bytes. All 72 corresponding native/runtime files were byte-identical, comparing 1,061,683,200 bytes once. All four channels of every readout passed the independently derived CPU reference intervals; this is not a claim that every CPU-predicted RGBA byte was unique or exact. Original/diagnostic B/A comparisons were byte-exact: 265,420,800 bytes at the source and another 265,420,800 bytes at the BLIT destination, each comparison counted once across 36 paired mode/case observations.

The actual pixels distinguish the transfer stages. The mutable UNORM image with an SRGB view and the native SRGB image with that same view produced identical source bytes in every case and shader version. The mutable path's BLIT preserved those source bytes exactly. The native SRGB path's BLIT changed RGB while preserving alpha, including decoding the raw nonzero LOAD pixels outside the draw. These differences follow the owned image/view paths; they do not observe a game attachment.

In the input-rectangle case, complete diagnostic R/G planes were byte-identical to those from the different texture A, in all three paths at both readout endpoints. R depended only on horizontal position and G only on vertical position, while sampled B retained its rectangle and alpha varied. Full-source and destination checks also passed for the negative-height viewport and the separate viewport/scissor LOAD controls. All six families of fixed wrong-model witnesses had rejecting locations. The 17 physical and final source fingerprints were identical. Khronos callbacks recorded zero errors and 48 loader warnings: three in each native group and five in each Steam Runtime group.

A separate CPU review checked every physical readout using a 70-digit Decimal/Fraction reference without calling the author's pixel or transfer functions. It reused the strict metadata parser with additional independent format checks, reconstructed the candidate byte-for-byte, and rehashed 361 physical files before and after. It also reconstructed both saved environment audits exactly from those independent measurements. Four additional single-channel corruptions made only in memory were refused. This review launched no process, GPU query or UI action and changed no physical output or saved author audit.

## Limits

This fixture owns every resource and placement. It observes no game image view, descriptor placement, UBO contents or GPU read. Even a successful SRGB model comparison would not establish that a candidate last wrote a game pixel, that downstream operations preserve its channels, or that game coordinates are finite. Numerical game UV decoding, game coverage, rectangle cause, XR submission and a rendering repair remain unestablished. No new game run is required for this owned control.
