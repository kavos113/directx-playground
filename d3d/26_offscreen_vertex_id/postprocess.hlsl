Texture2D<float4> tex : register(t0);
SamplerState samplerState : register(s0);

struct vs_output
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

/*
 * 0 -> (-1,  1) uv: (0, 0)
 * 1 -> ( 1,  1) uv: (1, 0)
 * 2 -> (-1, -1) uv: (0, 1)
 * 3 -> ( 1, -1) uv: (1, 1)
 */
vs_output vs_main(
    uint vertexId : SV_VertexID
)
{
    vs_output output;
    output.uv = float2(vertexId & 1, vertexId >> 1);
    output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    output.position.y *= -1.0;
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