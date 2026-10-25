#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(push_constant) uniform Item{vec4 position;vec4 color;}item;
layout(buffer_reference,std430,buffer_reference_align=16) readonly buffer Eye{uvec4 header;uvec4 identity;vec4 view[4];vec4 projection[4];vec4 gate;};
layout(constant_id=4)const uint64_t eye_address=0ul;
layout(location=0)flat out vec4 tint;
void main(){
    vec2 corners[3]=vec2[3](vec2(-.8,-.8),vec2(.8,-.8),vec2(0,.8));
    vec4 p=vec4(corners[gl_VertexIndex]*item.position.w+item.position.xy,-item.position.z,1);
    if(eye_address!=0ul){Eye e=Eye(eye_address);p=vec4(dot(e.view[0],p),dot(e.view[1],p),dot(e.view[2],p),dot(e.view[3],p));}
    gl_Position=vec4(1.784022*p.x,3.171595*p.y,-1.001001*p.z-.1001001,-p.z);tint=item.color;
}
