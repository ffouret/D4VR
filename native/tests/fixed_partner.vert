#version 460
#extension GL_EXT_buffer_reference : require
// Match the pinned helper's address model without reading a physical pointer.
layout(buffer_reference,std430) readonly buffer AddressModel {float x;};
layout(set=3,binding=0,std140) uniform Camera {vec4 rows[113];} camera;
layout(set=3,binding=1,std140) uniform Model {vec4 rows[80];} modelview;
layout(location=0) out vec3 sourceWorld;
layout(location=1) out vec4 partnerClip;
layout(location=2) out vec4 sourceView;
void main(){
    vec2 point=gl_VertexIndex==0 ? vec2(-.8,-.8) : gl_VertexIndex==1 ? vec2(.8,-.8) : vec2(0,.8);
    vec4 v=vec4(point,.5,1);
    vec4 p=vec4(dot(modelview.rows[56],v),dot(modelview.rows[57],v),dot(modelview.rows[58],v),dot(modelview.rows[59],v));
    sourceView=p;
    sourceWorld=vec3(dot(camera.rows[42],p),dot(camera.rows[43],p),dot(camera.rows[44],p));
    gl_Position=vec4(dot(camera.rows[72],p),dot(camera.rows[73],p),dot(camera.rows[74],p),dot(camera.rows[75],p));
    vec4 q=vec4(dot(modelview.rows[48],v),dot(modelview.rows[49],v),dot(modelview.rows[50],v),dot(modelview.rows[51],v));
    partnerClip=vec4(dot(camera.rows[80],q),dot(camera.rows[81],q),dot(camera.rows[82],q),dot(camera.rows[83],q));
}
