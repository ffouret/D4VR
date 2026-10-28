#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(set = 3, binding = 0, std140) uniform FixtureCamera { vec4 rows[113]; } camera;
layout(set = 3, binding = 1, std140) uniform FixtureModelView { vec4 rows[80]; } modelview;
layout(buffer_reference, std430) readonly buffer OptionalData { float x; };
layout(constant_id = 16) const uint64_t optional_address = 0ul;
void main()
{
    vec2 vertices[3] = vec2[3](vec2(-0.8, -0.8), vec2(0.8, -0.8), vec2(0.0, 0.8));
    vec4 position = vec4(vertices[gl_VertexIndex], 0.5, 1.0);
    position = vec4(dot(modelview.rows[56], position), dot(modelview.rows[57], position),
                    dot(modelview.rows[58], position), dot(modelview.rows[59], position));
    gl_Position = vec4(dot(camera.rows[72], position), dot(camera.rows[73], position),
                       dot(camera.rows[74], position), dot(camera.rows[75], position));
    if (optional_address != 0ul)
        gl_Position.x += OptionalData(optional_address).x;
}
