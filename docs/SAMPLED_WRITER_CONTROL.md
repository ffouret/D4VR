# Controlled sampled-image writer

The retained source prefixes identify a specific unmodified draw feeding the
captured color source. Its locally verified fragment module directly writes one
image sample. The actual game's selected image, sampler, UV offset and effective
viewport/scissor are still unknown. This control tests those alternatives on
owned inputs before adding another game observation.

`native/tests/sampled_writer_gpu.cpp` accepts private vertex and fragment SPIR-V
paths at runtime. It does not instrument them, install an override or launch the
game. CI compiles the fixture without those private inputs. The separate
`tools/sampled_writer_validate.py` verifies their expected hashes for a private
run and compares readbacks to an independent integer CPU reference.

## Inputs and reference

Two different initialized 8×8 RGBA8 textures occupy indices zero and one of the
sampled-image array. Two nearest samplers select clamp-to-edge or repeat, with
one mip and LOD zero. All descriptors used by the fixture are initialized;
descriptor queries or opaque descriptor bytes from the game are not involved.

The image index comes from push-constant word two, the sampler index from word
six, and the primary UV offset from the first two floats of a zero-initialized
1,280-byte UBO. Offsets use exact multiples of 1/8. The target is an owned
2560×1440 BGRA8 attachment initialized to RGBA `(3,5,7,255)`, then used with
LOAD/STORE. Three procedural vertices, known topology, disabled culling,
depth/stencil and blending, and a full color write mask make the fixture's
coverage explicit.

For a pixel center inside the declared viewport and scissor, the independent
reference computes:

```
u = (x + 0.5 - viewport.x) / viewport.width  + offset.x
v = 1 - (y + 0.5 - viewport.y) / viewport.height + offset.y
texel = floor(8 * (u, v))
```

Texel indices are clamped or reduced modulo eight, then the known RGBA texel is
packed into BGRA bytes. Pixels outside the fixture's viewport/scissor retain
the initialized LOAD sentinel. Extents divisible by eight and binary offsets
keep the chosen pixel centers away from nearest-sampling boundaries.

## Discriminating controls

The normal cases independently change the selected texture, the UV offset and
the sampler. A central scissor preserves the sentinel outside its rectangle;
a central viewport additionally rescales the image within that rectangle.
These can produce different rectangular color patterns even though the render
area itself covers the complete attachment.

Explicit wrong-field controls write an image index to an unused push-constant
word, move the UV offset into the other UBO components, or write the sampler
index to the wrong word. Their readbacks must match the reference for the
actual bytes supplied and differ from the intended reference. Raw push bytes,
UBO bytes, texture uploads and exact raster state are retained privately with
each run. The auditor does not accept an image hash supplied by the GPU fixture
as its reference.

The [executed control](proofs/sampled-writer-control-v1.json) compares eighteen
readbacks in native Vulkan and Steam Runtime: 265,420,800 BGRA bytes match the
actual-state CPU references exactly. All three wrong-field cases fail their
intended references, and both coverage witnesses remain distinguishable.
Synchronization validation reports zero errors. The callbacks also retain
three native and five runtime loader notices; the proof does not claim zero
warnings. No game or headset is launched for this step.

## Limits and next observation

This is a controlled draw with owned textures, not a reconstruction of the
game's sampled resource path. It does not establish the cause of Diablo's
central rectangle or wide-field cutouts. No game shader is rewritten, no bundle
is admitted, and no image is sent to OpenXR or the Quest.

The next observer must retain the original draw arguments, bounded push-constant
history and exact viewport/scissor declarations by value in the existing
capture-owned prefix. Static, dynamic and unsupported state must remain
distinct, including pipeline-layout lifetimes and compatibility. Push indices
alone do not identify a sampled image, and a descriptor query is not proof of
placement or a shader read. UBO values require an independently established
resource, lifetime and safe finite observation window; a general mapped-memory
scan is outside this diagnostic.
