#version 460
#extension GL_EXT_buffer_reference : require
// Match VKD3D’s PhysicalStorageBuffer64 addressing model without dereferencing an address.
layout(buffer_reference, std430) readonly buffer MemoryModelOnly { float unused; };

// Diagnostic only: move the eye by 0.10 view-space units for the measured
// 35-degree 16:9 projection. Other projection shapes are left unchanged.
void d4vr_geometry(uint vertex_id, vec4 p0, vec4 p1, vec4 p2, vec4 p3,
                   vec4 object0, vec4 object1, vec4 object2, vec4 object3,
                   inout vec4 clip_position)
{
    if (abs(p0.x - 1.784022) > 0.00001 || abs(p1.y - 3.171595) > 0.00001
        || abs(p3.z + 1.0) > 0.000001 || abs(p3.w) > 0.000001
        || abs(p0.y) + abs(p0.w) + abs(p1.x) + abs(p1.w)
         + abs(p2.x) + abs(p2.y) + abs(p3.x) + abs(p3.y) > 0.000001
        || abs(p2.w) < 0.000001)
        return;
    // Invert the depth part of the original projection to recover view W.
    // The resulting screen displacement depends on each vertex's depth.
    float view_w = (clip_position.z + p2.z * clip_position.w) / p2.w;
    clip_position.x -= p0.x * 0.10 * view_w;
}

void main()
{
    vec4 position = vec4(0);
    d4vr_geometry(uint(gl_VertexIndex), vec4(1), vec4(1), vec4(1), vec4(1),
                   vec4(1), vec4(1), vec4(1), vec4(1), position);
    gl_Position = position;
}
