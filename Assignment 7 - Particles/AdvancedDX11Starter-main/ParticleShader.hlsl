struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 colorTint : COLOR;
};

// Textures and such
Texture2D Particle : register(t0);
SamplerState BasicSampler : register(s0);

// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
{
    return Particle.Sample(BasicSampler, input.uv) * input.colorTint;
}