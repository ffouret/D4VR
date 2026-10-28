#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(push_constant) uniform Item {vec4 positions[3];vec4 color;} item;
layout(buffer_reference,std430,buffer_reference_align=16) readonly buffer Eye {uvec4 header;uvec4 identity;vec4 view[4];vec4 projection[4];vec4 gate;};
layout(constant_id=4) const uint64_t eye_address=0ul;
layout(location=0) flat out vec4 tint;
void main(){
    vec4 p=item.positions[gl_VertexIndex];
    if(eye_address!=0ul){
        Eye e=Eye(eye_address);
        p=vec4(dot(e.view[0],p),dot(e.view[1],p),dot(e.view[2],p),dot(e.view[3],p));
        p=vec4(dot(e.projection[0],p),dot(e.projection[1],p),dot(e.projection[2],p),dot(e.projection[3],p));
    }
    gl_Position=p;tint=item.color;
}
