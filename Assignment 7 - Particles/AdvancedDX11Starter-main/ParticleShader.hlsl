

cbuffer externalData : register(b0)
{
    float3 colorTint;
}

struct VertexToPixel
{
    float4 screenPosition : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

Texture2D particle : register(t0);
SamplerState basicSampler : register(s0);






float4 main(VertexToPixel input) : SV_TARGET
{
    float4 color = particle.Sample(basicSampler, input.uv) * input.color;
    color.rgb *= colorTint;
	return color;
}