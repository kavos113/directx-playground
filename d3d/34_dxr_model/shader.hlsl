RaytracingAccelerationStructure sceneAS : register(t0);
RWTexture2D<float4> output : register(u0);

struct Vertex 
{
    float3 position;
    float3 normal;
    float2 texcoord;
};

StructuredBuffer<Vertex> vertices : register(t1);
ByteAddressBuffer indices : register(t2);
Texture2D<float4> albedo : register(t3);

SamplerState sam : register(s0);

struct Payload 
{
    float4 color;
};

[shader("raygeneration")]
void RayGen() 
{
    uint2 dispatchIndex = DispatchRaysIndex().xy;
    uint2 targetSize = DispatchRaysDimensions().xy;

    float2 uv = (float2(dispatchIndex) / float2(targetSize)) * 2.0f - 1.0f; 
    uv.y = -uv.y; 

    RayDesc ray;
    ray.Origin = float3(0.0f, 0.0f, -2.0f); // Camera position
    ray.Direction = normalize(float3(uv, 1.0f)); // Ray direction
    ray.TMin = 0.001f;
    ray.TMax = 1000.0f; 

    Payload payload;
    payload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);

    TraceRay(
        sceneAS, 
        RAY_FLAG_NONE, 
        0xFF, 
        0, 
        0, 
        0,
        ray, 
        payload
    );

    output[dispatchIndex] = payload.color; 
}

[shader("miss")]
void MissShader(inout Payload payload) 
{
    payload.color = float4(0.4f, 0.6f, 0.8f, 1.0f);
}


[shader("closesthit")]
void ClosestHitShader(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint index = PrimitiveIndex();

    uint i0 = indices.Load((index * 3 + 0) * 4);
    uint i1 = indices.Load((index * 3 + 1) * 4);
    uint i2 = indices.Load((index * 3 + 2) * 4);

    float2 v0 = vertices[i0].texcoord;
    float2 v1 = vertices[i1].texcoord;
    float2 v2 = vertices[i2].texcoord;

    float2 uv = v0 + attr.barycentrics.x * (v1 - v0) + attr.barycentrics.y * (v2 - v0);

    float4 albedoColor = albedo.SampleLevel(sam, uv, 0);

    payload.color = albedoColor;
}

struct ShadowPayload 
{
    bool hit;
};

[shader("closesthit")]
void PlaneClosestHitShader(inout Payload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float t = RayTCurrent();
    float3 worldDir = WorldRayDirection();
    float3 worldOrigin = WorldRayOrigin();

    float3 worldPos = worldOrigin + t * worldDir;

    RayDesc ray;
    ray.Origin = worldPos;
    ray.Direction = normalize(float3(0.5f, 0.5f, -0.5f)); // light direction
    ray.TMin = 0.001f;
    ray.TMax = 100000.0f;

    ShadowPayload shadowPayload;

    TraceRay(
        sceneAS,
        RAY_FLAG_NONE,
        0xFF,
        1,
        0,
        1,
        ray,
        shadowPayload
    );

    float factor = shadowPayload.hit ? 0.1f : 1.0f;
    payload.color = float4(0.9f, 0.9f, 0.9f, 1.0f) * factor;
    payload.color.a = 1.0f; 
}

[shader("closesthit")]
void ShadowClosestHitShader(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.hit = true;
}


[shader("miss")]
void ShadowMissShader(inout ShadowPayload payload)
{
    payload.hit = false;
}