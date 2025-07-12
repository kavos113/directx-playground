Texture2D<float4> tex : register(t0);
SamplerState samplerState : register(s0);

cbuffer MatrixBuffer : register(b0)
{
    matrix worldMat;
    matrix view;
    matrix projection;
};

cbuffer LightBuffer : register(b1)
{
    float4 lightDirection;
    float4 ambientColor;
};

struct vs_output
{
    float4 position : SV_POSITION;
    float4 normal : NORMAL;
    float2 uv : TEXCOORD;
};

vs_output vs_main(
    float4 position : POSITION,
    float4 normal : NORMAL,
    float2 texCoord : TEXCOORD,
    float4x4 world : WORLD
)
{
    vs_output output;
    output.position = mul(projection, mul(view, mul(world, position)));
    output.normal = mul(world, normal);
    output.uv = texCoord;
    return output;
}

float4 ps_main(vs_output input) : SV_TARGET
{
    float4 color = tex.Sample(samplerState, input.uv);

    float4 ambient = ambientColor * color;

    float3 light = normalize(lightDirection.xyz);
    float3 normal = normalize(input.normal.xyz);
    float intensity = max(0, dot(normal, -light));

    float4 diffuse = color * intensity;

    return ambient + diffuse;
}