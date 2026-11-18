#include "Scene.hlsli"
Texture2D albedo : register(t0);
SamplerState linearSampler : register(s0);

float4 main(SceneVertex input) : SV_Target
{
    float3 normal = normalize(input.normal);
    float3 toEye = normalize(eye.xyz - input.world);
    float3 result = hasTexture ? albedo.Sample(linearSampler, input.uv).rgb : diffuse.rgb * 0.2;
    for (uint i = 0; i < lightCount; ++i)
    {
        LightData light = lights[i];
        float3 direction = normalize(-light.directionInner.xyz);
        float attenuation = 1;
        if (light.positionType.w > 0)
        {
            float3 delta = light.positionType.xyz - input.world;
            float distance = length(delta);
            float3 toLight = delta / max(distance, 0.00001);
            attenuation = rcp(max(dot(light.falloffOuter.xyz, float3(1, distance, distance * distance)), 0.00001));
            if (light.positionType.w == 2)
                attenuation *= saturate((dot(toLight, direction) - light.falloffOuter.w)
                    / max(light.directionInner.w - light.falloffOuter.w, 0.00001));
            direction = toLight;
        }
        float lambert = saturate(dot(normal, direction));
        float spec = pow(saturate(dot(toEye, reflect(-direction, normal))), specularShininess.w);
        result += (diffuse.rgb * lambert + specularShininess.rgb * spec)
            * light.colorIntensity.rgb * light.colorIntensity.w * attenuation;
    }
    return float4(result, diffuse.a);
}
