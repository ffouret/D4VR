#version 450
layout(push_constant) uniform P { float depth; } p;
void main(){vec2 positions[3]=vec2[](vec2(-.7,-.7),vec2(.7,-.7),vec2(0,.7));gl_Position=vec4(positions[gl_VertexIndex],p.depth,1);}
