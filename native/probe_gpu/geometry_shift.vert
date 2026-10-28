#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(constant_id = 0) const uint64_t shader_id = 0ul;
layout(constant_id = 1) const uint64_t control_address = 0ul;
layout(constant_id = 2) const uint64_t ring_address = 0ul;
layout(constant_id = 3) const uint ring_words = 1u;
layout(buffer_reference, std430, buffer_reference_align = 4) coherent buffer Counters {
    uint allocated_words;
    uint sampled_invocations;
};
layout(buffer_reference, std430, buffer_reference_align = 4) coherent buffer DataRing { uint words[]; };

void d4vr_record_geometry(uint vertex_id, vec4 before, vec4 after, vec4 projection, vec4 status)
{
    if (vertex_id != 0u || control_address == 0ul || ring_address == 0ul || ring_words < 64u)
        return;
    // Stop incrementing once the budget is exhausted, preventing counter wrap.
    if (atomicAdd(Counters(control_address).sampled_invocations, 0u) >= 67108864u)
        return;
    uint invocation = atomicAdd(Counters(control_address).sampled_invocations, 1u);
    if ((invocation & 65535u) != 0u || invocation >= 67108864u)
        return;
    uint sample_id = invocation >> 16;
    uint start = atomicAdd(Counters(control_address).allocated_words, 48u);
    uint mask = ring_words - 1u;
    uint64_t runtime_shader_id = shader_id + uint64_t(vertex_id);
    DataRing stream = DataRing(ring_address);
    vec4 rows[4] = vec4[4](before, after, projection, status);
    for (uint row = 0u; row < 4u; ++row) {
        uint offset = start + row * 12u;
        stream.words[(offset + 1u) & mask] = uint(runtime_shader_id);
        stream.words[(offset + 2u) & mask] = uint(runtime_shader_id >> 32);
        stream.words[(offset + 3u) & mask] = sample_id;
        stream.words[(offset + 4u) & mask] = 0x44344745u;
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

// Diagnostic only: move the eye by 0.10 view-space units for the measured
// 35-degree 16:9 projection. Other projection shapes are left unchanged.
void d4vr_geometry(uint vertex_id, vec4 p0, vec4 p1, vec4 p2, vec4 p3,
                   vec4 object0, vec4 object1, vec4 object2, vec4 object3,
                   inout vec4 clip_position)
{
    vec4 before = clip_position;
    bool matched = abs(p0.x - 1.784022) <= 0.00001 && abs(p1.y - 3.171595) <= 0.00001
        && abs(p3.z + 1.0) <= 0.000001 && abs(p3.w) <= 0.000001
        && abs(p0.y) + abs(p0.w) + abs(p1.x) + abs(p1.w)
         + abs(p2.x) + abs(p2.y) + abs(p3.x) + abs(p3.y) <= 0.000001
        && abs(p2.w) >= 0.000001;
    float view_w = 0.0;
    if (matched) {
        view_w = (clip_position.z + p2.z * clip_position.w) / p2.w;
        clip_position.x -= p0.x * 0.10 * view_w;
    }
    d4vr_record_geometry(vertex_id, before, clip_position, vec4(p0.x, p1.y, p2.z, p2.w),
                        vec4(matched ? 1.0 : 0.0, view_w, 0.10, 0.0));

}

void main()
{
    vec4 position = vec4(0);
    d4vr_geometry(uint(gl_VertexIndex), vec4(1), vec4(1), vec4(1), vec4(1),
                   vec4(1), vec4(1), vec4(1), vec4(1), position);
    gl_Position = position;
}
