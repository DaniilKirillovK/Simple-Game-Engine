#include "Scene.hlsli"
SceneVertex main(float3 position : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
    SceneVertex output;
    float4 world = mul(model, float4(position, 1));
    output.position = mul(viewProjection, world);
    output.world = world.xyz;
    output.normal = normalize(mul((float3x3)normalMatrix, normal));
    output.uv = uv;
    return output;
}
