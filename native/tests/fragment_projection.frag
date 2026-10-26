#version 460
#extension GL_EXT_buffer_reference : require
layout(buffer_reference,std430) readonly buffer AddressModel {float x;};
layout(set=3,binding=0,std140) uniform Camera {vec4 rows[113];} camera;
layout(location=0) in vec3 sourceWorld;
layout(location=1) in vec4 partnerClip;
layout(location=2) in vec4 sourceView;
layout(location=0) out vec4 color;
void main(){
    vec4 current=vec4(dot(camera.rows[72],sourceView),dot(camera.rows[73],sourceView),dot(camera.rows[74],sourceView),dot(camera.rows[75],sourceView));
    vec2 raster=gl_FragCoord.xy/vec2(256)*2-1;
    if(any(greaterThan(abs(current.xy/current.w-raster),vec2(.02))))discard;
    vec2 uv=partnerClip.xy/partnerClip.w;
    color=vec4(sourceWorld.x<0 ? .125:.875, uv.x<.03 ? .25:.75,
        (uv.y<.02 ? .375:.625)+(partnerClip.z/partnerClip.w>.87 ? .125:0),1);
}
