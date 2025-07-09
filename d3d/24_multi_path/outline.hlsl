cbuffer MatrixBuffer : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
    float outlineWidth;
};

struct vs_output
{
    float4 position : SV_POSITION;
};

vs_output vs_main(
    float4 position : POSITION,
    float3 normal : NORMAL
)
{
    vs_output output;
    float4 pos = position;
    pos.xyz += normal * outlineWidth;
    output.position = mul(projection, mul(view, mul(world, pos)));
    return output;
}

float4 ps_main(vs_output input) : SV_TARGET
{
    return float4(1, 0, 0, 1);
}