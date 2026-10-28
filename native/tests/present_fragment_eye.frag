#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(buffer_reference,std430,buffer_reference_align=16) readonly buffer Eye{uvec4 header;uvec4 identity;vec4 view[4];vec4 projection[4];vec4 gate;};
layout(constant_id=4)const uint64_t eye_address=0ul;
layout(location=0)flat in vec4 tint;
layout(location=1)flat in vec4 vertexPacket;
layout(location=0)out vec4 color;
void main(){
    vec4 fragmentPacket=vec4(0);
    if(eye_address!=0ul){Eye e=Eye(eye_address);fragmentPacket=vec4(float(e.identity.x+1u),float(e.header.z),e.view[0].w,e.view[1].w);}
    color=any(notEqual(vertexPacket,fragmentPacket)) ? vec4(1,0,1,1) : tint;
}
