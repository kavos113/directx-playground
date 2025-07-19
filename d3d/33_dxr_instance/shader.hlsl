RaytracingAccelerationStructure sceneAS : register(t0);
RWTexture2D<float4> output : register(u0);

cbuffer Colors : register(b0)
{
    float4 colors[3];
};

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
    uint instanceID = InstanceID();

    payload.color = colors[instanceID % 3]; 
}