// Owned finite descriptor-copy reference; no original game shader is used.
Texture2D<float4> sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);
cbuffer Constants : register(b0) { float2 uvOffset; };

float4 VSMain(uint index : SV_VertexID) : SV_Position
{
    float2 corner = float2((index << 1) & 2, index & 2);
    return float4(corner * float2(2, -2) + float2(-1, 1), 0, 1);
}

float4 PSMain(float4 position : SV_Position) : SV_Target0
{
    float2 uv = position.xy / float2(256.0, 144.0) + uvOffset;
    return sourceTexture.SampleLevel(sourceSampler, uv, 0.0);
}
