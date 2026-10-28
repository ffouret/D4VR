#version 460
layout(location=0) noperspective in vec2 projected;
layout(location=0) out vec4 color;
void main(){
    vec2 raster=gl_FragCoord.xy/vec2(256)*2-1;
    if(any(greaterThan(abs(projected-raster),vec2(.02))))discard;
    color=vec4(.125,.5,.875,1);
}
