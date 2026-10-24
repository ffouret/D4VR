// Original synthetic fixture: known nonzero register space and matrix offset.
cbuffer SyntheticCamera : register(b3, space2)
{
    float4 padding[7];
    row_major float4x4 viewProjection;
    float4 unrelated;
};

cbuffer SyntheticObject : register(b1, space2)
{
    float4 objectPadding[2];
    row_major float4x4 modelView;
};

float4 main(float4 position : POSITION) : SV_Position
{
    return mul(viewProjection, mul(modelView, float4(position.xyz, 1.0)));
}
