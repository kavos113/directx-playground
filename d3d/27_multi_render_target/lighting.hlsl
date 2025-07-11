Texture2D<float4> albedoTex : register(t0);
Texture2D<float4> normalTex : register(t1);
SamplerState samplerState : register(s0);

cbuffer LightBuffer : register(b0)
{
    float4 lightDirection; 
    float4 lightColor;
};

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
    float4 albedo = albedoTex.Sample(samplerState, input.uv);
    float4 texNormal = normalTex.Sample(samplerState, input.uv);
    float3 normal = normalize(texNormal.xyz * 2.0 - 1.0); 

    float diffuse = saturate(dot(normal, -lightDirection.xyz));
    
    float3 color = albedo.rgb * lightColor.rgb * diffuse; 
    return float4(color, 1.0);
}