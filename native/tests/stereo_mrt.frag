#version 460
layout(location = 0) flat in vec4 tint;
layout(location = 0) out vec4 albedo;
layout(location = 1) out vec4 material;
layout(location = 2) out vec4 auxiliary;
layout(location = 3) out vec4 encoded;
void main()
{
    albedo = tint;
    material = vec4(.25+.5*tint.rgb,1);
    auxiliary = vec4(tint.bgr,1);
    encoded = vec4(tint.r*.7,tint.g*.8,tint.b*.9,1);
}
