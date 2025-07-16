struct Data
{
    float value;
    float id;
};

RWStructuredBuffer<Data> outputBuffer : register(u0);

[numthreads(4, 4, 4)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Data data;
    data.value = outputBuffer[dispatchThreadID.x + dispatchThreadID.y * 4 + dispatchThreadID.z * 16].value * 1.5f;
    data.id = dispatchThreadID.x + dispatchThreadID.y * 4 + dispatchThreadID.z * 16;

    outputBuffer[dispatchThreadID.x + dispatchThreadID.y * 4 + dispatchThreadID.z * 16] = data;
}