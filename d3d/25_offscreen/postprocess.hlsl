Texture2D<float4> tex : register(t0);
SamplerState samplerState : register(s0);

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
    output.position = position;
    output.uv = texCoord;
    return output;
}

float4 ps_main(vs_output input) : SV_TARGET
{
    // Sample the texture
    float4 color = tex.Sample(samplerState, input.uv);

    // Apply a simple post-processing effect (e.g., grayscale)
    float gray = dot(color.rgb, float3(0.299, 0.587, 0.114));
    return float4(gray, gray, gray, color.a);
}