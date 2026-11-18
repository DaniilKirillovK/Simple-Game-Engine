cbuffer Transform : register(b0) { float4x4 transform; }
struct Output
{
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};
Output main(float3 position : POSITION, float4 color : COLOR, float2 uv : TEXCOORD)
{
    Output output;
    output.position = mul(transform, float4(position, 1));
    output.color = color;
    output.uv = uv;
    return output;
}
