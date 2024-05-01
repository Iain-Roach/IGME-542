cbuffer externalData : register(b0)
{
    float2 pixelSize; // (1.0/windowWidth, 1.0/windowHeight)
}

struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D texture : register(t0);
SamplerState ClampSampler : register(s0);


float4 main(VertexToPixel input) : SV_TARGET
{
    float ao = 0;
    for (float x = -1.5f; x <= 1.5f; x++) // -1.5, -0.5, 0.5, 1.5
    {
        for (float y = -1.5f; y <= 1.5f; y++)
        {
            ao += texture.Sample(ClampSampler, float2(x, y) * pixelSize + input.uv).r;
        }
    }
// Average results and return
    ao /= 16.0f; // 4x4 blur is 16 samples
    return float4(ao.rrr, 1);
}