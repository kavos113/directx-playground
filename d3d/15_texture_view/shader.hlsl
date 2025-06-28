cbuffer cb : register(b0)
{
    float4 color;
};

struct vs_output
{
    float4 position : SV_POSITION;
};

vs_output vs_main(
    float4 position : POSITION
)
{
    vs_output output;
    output.position = position;
    return output;
}

float4 ps_main(vs_output input) : SV_TARGET
{
    return color;
}