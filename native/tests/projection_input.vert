#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(set=3,binding=0,std140) uniform Camera {vec4 rows[113];} camera;
layout(set=3,binding=1,std140) uniform Model {vec4 rows[80];} modelview;
layout(location=0) noperspective out vec2 projected;
layout(buffer_reference,std430) readonly buffer OptionalData {float x;};
layout(constant_id=16) const uint64_t optional_address=0ul;
void main(){
    vec2 vertices[3]=vec2[3](vec2(-.8,-.8),vec2(.8,-.8),vec2(0,.8));
    vec4 p=vec4(vertices[gl_VertexIndex],.5,1);
    p=vec4(dot(modelview.rows[56],p),dot(modelview.rows[57],p),dot(modelview.rows[58],p),dot(modelview.rows[59],p));
    p=vec4(dot(camera.rows[72],p),dot(camera.rows[73],p),dot(camera.rows[74],p),dot(camera.rows[75],p));
    gl_Position=p;
    // A second original computation uses the same projection. A late Position
    // edit cannot make this fragment input follow the requested viewpoint.
    projected=p.xy/p.w;
    if(optional_address!=0ul)gl_Position.x+=OptionalData(optional_address).x;
}
