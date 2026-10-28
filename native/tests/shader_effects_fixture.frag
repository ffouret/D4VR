#version 460
// Read-only image controls still write the fragment output attachment.
#ifdef D4VR_EFFECT_INPUT_ATTACHMENT
layout(input_attachment_index=0,set=0,binding=0) uniform subpassInput source;
#else
layout(set=0,binding=0) uniform sampler2D source;
#endif
layout(location=0) out vec4 color;
void main() {
#ifdef D4VR_EFFECT_INPUT_ATTACHMENT
    color=subpassLoad(source);
#else
    color=textureLod(source,vec2(.5),0);
#endif
}
