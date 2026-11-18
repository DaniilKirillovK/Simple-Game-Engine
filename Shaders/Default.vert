#include "Scene.hlsli"

// Per-instance transforms: instance i uses matrices [i*2] (model) and [i*2+1] (normal).
// Bound for every draw; only read when DrawConstants.instanced != 0.
StructuredBuffer<float4x4> instanceData : register(t1);

SceneVertex main(float3 position : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD, uint instanceId : SV_InstanceID)
{
    SceneVertex output;

    float4x4 modelMatrix = model;
    float3x3 normalMatrix3 = (float3x3)normalMatrix;
    if (instanced)
    {
        modelMatrix = instanceData[instanceId * 2];
        normalMatrix3 = (float3x3)instanceData[instanceId * 2 + 1];
    }

    float4 world = mul(modelMatrix, float4(position, 1));
    output.position = mul(viewProjection, world);
    output.world = world.xyz;
    output.normal = normalize(mul(normalMatrix3, normal));
    output.uv = uv;
    return output;
}
