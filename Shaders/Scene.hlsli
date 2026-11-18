struct LightData
{
    float4 colorIntensity;
    float4 positionType;
    float4 directionInner;
    float4 falloffOuter;
};
cbuffer DrawConstants : register(b0)
{
    float4x4 model;
    float4x4 viewProjection;
    float4x4 normalMatrix;
    float4 diffuse;
    float4 specularShininess;
    float4 eye;
    uint lightCount;
    uint hasTexture;
    uint instanced;
    uint padding;
    LightData lights[8];
};
struct SceneVertex
{
    float4 position : SV_Position;
    float3 world : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
};
