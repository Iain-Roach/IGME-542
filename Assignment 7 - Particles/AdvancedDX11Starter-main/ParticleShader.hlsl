struct VertexToPixel
{
    float4 screenPosition : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D Particle : register(t0);
SamplerState BasicSampler : register(s0);






float4 main(VertexToPixel input) : SV_TARGET
{
    float4 color = Particle.Sample(BasicSampler, input.uv);
	return color;
}