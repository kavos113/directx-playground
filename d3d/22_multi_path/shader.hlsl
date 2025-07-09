Texture2D<float4> tex : register(t0);
SamplerState samplerState : register(s0);

cbuffer MatrixBuffer : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
};

struct vs_output
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

vs_output vs_main(
    float4 position : POSITION,
    float2 texCoord : TEXCOORD
)
{
    vs_output output;
    output.position = mul(projection, mul(view, mul(world, position)));
    output.uv = texCoord;
    return output;
}

float4 ps_main(vs_output input) : SV_TARGET
{
    return tex.Sample(samplerState, input.uv);
}