cbuffer cb : register(b0)
{
    float4 color;
};

struct vs_output
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

vs_output vs_main(
    float4 position : POSITION,
    float4 color : COLOR
)
{
    vs_output output;
    output.position = position;
    output.color = color;
    return output;
}

float4 ps_main(vs_output input) : SV_TARGET
{
    return color;
}