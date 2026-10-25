#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
// Static control only: a telemetry-free helper must preserve this source write.
layout(set=3,binding=0,std140) uniform Camera { vec4 rows[113]; } camera;
layout(set=3,binding=1,std140) uniform ModelView { vec4 rows[80]; } modelview;
layout(set=2,binding=0,std430) buffer SourceEffect { uint value; } effect;
layout(buffer_reference,std430) readonly buffer OptionalData { float x; };
layout(constant_id=16) const uint64_t optional_address=0ul;
void main() {
    vec4 p=vec4(float(gl_VertexIndex),0,0,1);
    p=vec4(dot(modelview.rows[56],p),dot(modelview.rows[57],p),
           dot(modelview.rows[58],p),dot(modelview.rows[59],p));
    gl_Position=vec4(dot(camera.rows[72],p),dot(camera.rows[73],p),
                     dot(camera.rows[74],p),dot(camera.rows[75],p));
    effect.value=uint(gl_VertexIndex);
    if(optional_address!=0ul)gl_Position.x+=OptionalData(optional_address).x;
}
