#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// VKD3D's debug ring protocol at pinned build 0bb64c56c8f0dcc. No game textures are changed.
layout(constant_id = 0) const uint64_t shader_id = 0ul;
layout(constant_id = 1) const uint64_t control_address = 0ul;
layout(constant_id = 2) const uint64_t ring_address = 0ul;
layout(constant_id = 3) const uint ring_words = 1u;

layout(buffer_reference, std430, buffer_reference_align = 4) coherent buffer Counters {
    uint allocated_words;
    uint sampled_invocations;
};
layout(buffer_reference, std430, buffer_reference_align = 4) coherent buffer DataRing {
    uint words[];
};

void d4vr_probe(uint vertex_id, vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h)
{
    if (vertex_id != 0u || control_address == 0ul || ring_address == 0ul || ring_words < 128u)
        return;
    uint invocation = atomicAdd(Counters(control_address).sampled_invocations, 1u);
    if ((invocation & 65535u) != 0u || invocation >= 67108864u)
        return;
    uint sample_id = invocation >> 16;
    uint start = atomicAdd(Counters(control_address).allocated_words, 96u);
    uint mask = ring_words - 1u;
    // vertex_id is zero on this path. Keep conversion at runtime for SPIR-V 1.3.
    uint64_t runtime_shader_id = shader_id + uint64_t(vertex_id);
    DataRing stream = DataRing(ring_address);
    vec4 rows[8] = vec4[8](a, b, c, d, e, f, g, h);
    for (uint row = 0u; row < 8u; ++row) {
        uint offset = start + row * 12u;
        stream.words[(offset + 1u) & mask] = uint(runtime_shader_id);
        stream.words[(offset + 2u) & mask] = uint(runtime_shader_id >> 32);
        stream.words[(offset + 3u) & mask] = sample_id;
        stream.words[(offset + 4u) & mask] = 0x44345653u;
        stream.words[(offset + 5u) & mask] = sample_id;
        stream.words[(offset + 6u) & mask] = row;
        stream.words[(offset + 7u) & mask] = 0xaaaaaaaau;
        uvec4 bits = floatBitsToUint(rows[row]);
        for (uint component = 0u; component < 4u; ++component)
            stream.words[(offset + 8u + component) & mask] = bits[component];
        memoryBarrierBuffer();
        stream.words[offset & mask] = 0xdeadca7cu;
        memoryBarrierBuffer();
    }
}

// Removed by the preparation tool; keeps the helper compilable with glslang.
void main()
{
    d4vr_probe(uint(gl_VertexIndex), vec4(1), vec4(1), vec4(1), vec4(1), vec4(1), vec4(1), vec4(1), vec4(1));
    gl_Position = vec4(0);
}
