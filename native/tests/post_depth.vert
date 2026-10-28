#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
// Owned finite-input suffix fixture. Set D4VR_POST_DEPTH_BIAS=1 for Z+k/W.
#ifndef D4VR_POST_DEPTH_BIAS
#define D4VR_POST_DEPTH_BIAS 0
#endif
layout(set=3,binding=0,std140) uniform Camera {vec4 rows[113];} camera;
layout(set=3,binding=1,std140) uniform Model {vec4 rows[80];} modelview;
layout(location=0) noperspective out vec2 projected;
// Select the same physical-storage addressing model as the helper library.
// No address, storage read, or extra specialization constant is needed.
layout(buffer_reference,std430) readonly buffer UnusedAddressModel {float unused;};
void main(){
    float top=float(gl_VertexIndex>>1);
    float side=float(gl_VertexIndex&1)*2-1;
    vec4 p=vec4(side*.8*(1-top),top*1.6-.8,side*.15*(1-top),1);
    p=vec4(dot(modelview.rows[56],p),dot(modelview.rows[57],p),dot(modelview.rows[58],p),dot(modelview.rows[59],p));
    float qx=dot(camera.rows[72],p),qy=dot(camera.rows[73],p);
    float qz=dot(camera.rows[74],p),qw=dot(camera.rows[75],p);
    // This independent original varying also detects a late Position rewrite.
    // The fragment has no discard: alpha is an independent coverage mask.
    projected=vec2(qx,qy)/qw;
#if D4VR_POST_DEPTH_BIAS
    precise float k=camera.rows[51].x*0.1+0.33;
    gl_Position=vec4(qx,qy,qz+k/qw,qw);
#else
    // The validator normalizes this owned finite-input FMin to NMin (79),
    // matching the suffix operator under examination, then runs spirv-val.
    gl_Position=vec4(qx,qy,min(qz,qw),qw);
#endif
}
