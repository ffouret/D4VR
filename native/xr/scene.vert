#version 450
layout(push_constant) uniform Camera { mat4 mvp; } camera;
layout(location=0) out vec3 color;
layout(location=1) out vec3 local_position;
layout(location=2) flat out uint floor_surface;
void main() {
    const vec3 corners[8] = vec3[8](vec3(-1,-1,-1),vec3(1,-1,-1),vec3(1,1,-1),vec3(-1,1,-1),
                                  vec3(-1,-1,1),vec3(1,-1,1),vec3(1,1,1),vec3(-1,1,1));
    const int indices[36] = int[36](0,1,2,0,2,3,4,6,5,4,7,6,0,4,5,0,5,1,
                                    3,2,6,3,6,7,1,5,6,1,6,2,0,3,7,0,7,4);
    const vec3 faces[6] = vec3[6](vec3(.2,.5,.9),vec3(.3,.7,1),vec3(.15,.3,.5),
                                  vec3(.6,.85,1),vec3(.2,.65,.8),vec3(.25,.45,.7));
    const vec2 floor_points[6] = vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
    floor_surface = gl_VertexIndex >= 36 ? 1u : 0u;
    vec3 p;
    if (floor_surface == 0u) { p = corners[indices[gl_VertexIndex]] * .18; color = faces[gl_VertexIndex / 6]; }
    else { vec2 f = floor_points[gl_VertexIndex-36] * .8; p = vec3(f.x, -.18, f.y); color = vec3(.15); }
    local_position = p;
    gl_Position = camera.mvp * vec4(p,1);
}
