#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#ifndef D4VR_DIAGNOSTIC_TELEMETRY
#define D4VR_DIAGNOSTIC_TELEMETRY 1
#endif
#if D4VR_DIAGNOSTIC_TELEMETRY != 0 && D4VR_DIAGNOSTIC_TELEMETRY != 1
#error D4VR_DIAGNOSTIC_TELEMETRY must be zero or one
#endif
#if D4VR_DIAGNOSTIC_TELEMETRY
layout(constant_id = 0) const uint64_t shader_id = 0ul;
layout(constant_id = 1) const uint64_t control_address = 0ul;
layout(constant_id = 2) const uint64_t ring_address = 0ul;
layout(constant_id = 3) const uint ring_words = 1u;
layout(buffer_reference, std430, buffer_reference_align = 4) coherent buffer Counters {
    uint allocated_words;
    uint sampled_invocations;
};
layout(buffer_reference, std430, buffer_reference_align = 4) coherent buffer DataRing { uint words[]; };
#endif
layout(constant_id = 4) const uint64_t eye_state_address = 0ul;
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer EyeState {
    uvec4 header;
    uvec4 identity;
    vec4 eye_from_game_view[4];
    vec4 eye_projection[4];
    vec4 source_gate;
};

#if D4VR_DIAGNOSTIC_TELEMETRY
void d4vr_record_stereo(uint vertex_id, vec4 before, vec4 after, vec4 projection, vec4 status)
{
    if (vertex_id != 0u || control_address == 0ul || ring_address == 0ul || ring_words < 64u)
        return;
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
        stream.words[(offset + 4u) & mask] = 0x44345354u;
        stream.words[(offset + 5u) & mask] = sample_id;
        stream.words[(offset + 6u) & mask] = row;
        stream.words[(offset + 7u) & mask] = 0xaaaaaaaau;
        uvec4 bits = floatBitsToUint(rows[row]);
        for (uint component = 0u; component < 4u; ++component)
            stream.words[(offset + 8u + component) & mask] = bits[component];
        memoryBarrierBuffer(); stream.words[offset & mask] = 0xdeadca7cu; memoryBarrierBuffer();
    }
}
#endif

// Recover the game's view-space homogeneous position using its current
// projection, including jitter and variable depth coefficients. Transform the
// geometry before rasterization; no screen/depth image is sampled.
void d4vr_stereo(uint vertex_id, vec4 p0, vec4 p1, vec4 p2, vec4 p3,
                 vec4 object0, vec4 object1, vec4 object2, vec4 object3,
                 inout vec4 clip_position)
{
    vec4 before = clip_position;
    bool applied = false;
    uint eye = 0u, sequence = 0u;
    if (eye_state_address != 0ul) {
        EyeState state = EyeState(eye_state_address);
        uvec4 header = state.header;
        eye = state.identity.x; sequence = header.z;
        vec4 gate = state.source_gate;
        bool valid = header.x == 0x44345652u && header.y == 1u && header.w <= 3u && (header.w & 1u) != 0u
            && sequence > 0u && sequence <= 0xffffffu && eye < 2u
            && gate.x > 0.0 && gate.y > 0.0 && gate.z > 0.0 && gate.z <= 0.001;
        bool matched = valid && abs(p0.x - gate.x) <= gate.z && abs(p1.y - gate.y) <= gate.z
            && abs(p3.z + 1.0) <= 0.000001 && abs(p3.w) <= 0.000001
            && abs(p0.x) >= 0.000001 && abs(p1.y) >= 0.000001
            && abs(p0.y) + abs(p0.w) + abs(p1.x) + abs(p1.w)
             + abs(p2.x) + abs(p2.y) + abs(p3.x) + abs(p3.y) <= 0.000001
            && abs(p2.w) >= 0.000001;
        if (matched) {
            vec4 view = vec4((before.x + p0.z * before.w) / p0.x,
                             (before.y + p1.z * before.w) / p1.y,
                             -before.w, (before.z + p2.z * before.w) / p2.w);
            vec4 eye_view = vec4(dot(state.eye_from_game_view[0], view), dot(state.eye_from_game_view[1], view),
                                 dot(state.eye_from_game_view[2], view), dot(state.eye_from_game_view[3], view));
            if ((header.w & 2u) != 0u)
                clip_position = vec4(dot(p0,eye_view),dot(p1,eye_view),dot(p2,eye_view),dot(p3,eye_view));
            else
                clip_position = vec4(dot(state.eye_projection[0], eye_view), dot(state.eye_projection[1], eye_view),
                                      dot(state.eye_projection[2], eye_view), dot(state.eye_projection[3], eye_view));
            applied = true;
        }
    }
#if D4VR_DIAGNOSTIC_TELEMETRY
    d4vr_record_stereo(vertex_id, before, clip_position, vec4(p0.x, p1.y, p2.z, p2.w),
                       vec4(applied ? 1.0 : 0.0, float(eye), float(sequence), 1.0));
#endif
}

void main()
{
    vec4 position = vec4(0);
    d4vr_stereo(uint(gl_VertexIndex), vec4(1), vec4(1), vec4(1), vec4(1),
                 vec4(1), vec4(1), vec4(1), vec4(1), position);
    gl_Position = position;
}
