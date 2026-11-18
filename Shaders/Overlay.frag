Texture2D image : register(t0);
SamplerState linearSampler : register(s0);
float4 main(float4 position : SV_Position, float4 color : COLOR, float2 uv : TEXCOORD) : SV_Target
{
    return color * image.Sample(linearSampler, uv);
}
