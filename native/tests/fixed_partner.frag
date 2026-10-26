#version 460
layout(location=0) in vec3 sourceWorld;
layout(location=1) in vec4 partnerClip;
layout(location=0) out vec4 color;
void main(){
    // Piecewise constant output makes both source paths visible without
    // treating float rounding at a boundary as a continuous-color mismatch.
    vec2 uv=partnerClip.xy/partnerClip.w;
    float partnerDepth=partnerClip.z/partnerClip.w;
    color=vec4(sourceWorld.x<0 ? .125:.875, uv.x<.03 ? .25:.75,
        (uv.y<.02 ? .375:.625)+(partnerDepth>.87 ? .125:0),1);
}
