#version 460
layout(location=0) noperspective in vec2 projected;
layout(location=0) out vec4 color;
void main(){color=vec4(.35+.2*projected.x,.45+.2*projected.y,.65,1);}
