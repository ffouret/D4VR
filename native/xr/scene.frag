#version 450
layout(location=0) in vec3 color;
layout(location=1) in vec3 local_position;
layout(location=2) flat in uint floor_surface;
layout(location=0) out vec4 output_color;
void main() {
    vec3 c = color;
    if (floor_surface != 0u) {
        vec2 grid = abs(fract(local_position.xz * 5 + .5) - .5);
        float line = 1 - smoothstep(.015, .03, min(grid.x,grid.y));
        c = mix(vec3(.08,.10,.12),vec3(.3,.4,.45),line);
    }
    output_color = vec4(c,1);
}
