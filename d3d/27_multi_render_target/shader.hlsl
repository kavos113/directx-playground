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
    float4 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct ps_output
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

vs_output vs_main(
    float4 position : POSITION,
    float4 normal : NORMAL,
    float2 texCoord : TEXCOORD
)
{
    vs_output output;
    output.position = mul(projection, mul(view, mul(world, position)));
    output.normal = mul(world, normal);
    output.uv = texCoord;
    return output;
}

ps_output ps_main(vs_output input)
{
    ps_output output;
    output.albedo = tex.Sample(samplerState, input.uv);

    float3 normalizedNormal = normalize(input.normal.xyz);
    output.normal = float4(normalizedNormal * 0.5 + 0.5, 1.0);
    return output;
}