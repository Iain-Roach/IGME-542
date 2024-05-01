#include "Lighting.hlsli"

cbuffer data : register(b0)
{
    float roughness;
    int faceIndex;
    int mipLevel;
}

struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

TextureCube EnvironmentMap : register(t0);
SamplerState BasicSampler : register(s0);

float3 PrefilterEnvMap(float Roughness, float3 R)
{
    float3 N = R;
    float3 V = R;
    float3 PrefilteredColor = 0;
    const uint NumSamples = 1024;
    float totalWeight = 0;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, Roughness, N);
        float3 L = 2 * dot(V, H) * H - V;
        float NoL = saturate(dot(N, L));
        if (NoL > 0)
        {
            float3 thisColor = EnvironmentMap.SampleLevel(BasicSampler, L, 0).rgb;
            thisColor = pow(abs(thisColor), 2.2f);
            PrefilteredColor += thisColor * NoL;
            totalWeight += NoL;
        }
    }
    return pow(abs(PrefilteredColor / totalWeight), 1.0f / 2.2f);
}

float4 main(VertexToPixel input) : SV_TARGET
{
	// Get a -1 to 1 range on x/y
    float2 o = input.uv * 2 - 1;

	// Tangent basis
    float3 xDir, yDir, zDir;

	// Figure out the z ("normal" of this pixel)
    switch (faceIndex)
    {
        default:
        case 0:
            zDir = float3(+1, -o.y, -o.x);
            break;
        case 1:
            zDir = float3(-1, -o.y, +o.x);
            break;
        case 2:
            zDir = float3(+o.x, +1, +o.y);
            break;
        case 3:
            zDir = float3(+o.x, -1, -o.y);
            break;
        case 4:
            zDir = float3(+o.x, -o.y, +1);
            break;
        case 5:
            zDir = float3(-o.x, -o.y, -1);
            break;
    }
    zDir = normalize(zDir);

	// Process the convolution for the direction of this pixel
    float3 c = PrefilterEnvMap(roughness, zDir);
    return float4(c, 1);
}