#include "Lighting.hlsli"

struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

// Specular G
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
float G1_Schlick(float Roughness, float NdotV)
{
    float k = Roughness * Roughness;
    k /= 2.0f; // Schlick-GGX version of k - Used in UE4

	// Staying the same
    return NdotV / (NdotV * (1.0f - k) + k);
}

// Specular G
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
float G_Smith(float Roughness, float NdotV, float NdotL)
{
    return G1_Schlick(Roughness, NdotV) * G1_Schlick(Roughness, NdotL);
}

float2 IntegrateBRDF(float Roughness, float NoV)
{
    float3 V;
    V.x = sqrt(1.0f - NoV * NoV); // sin
    V.y = 0;
    V.z = NoV; // cos
    
    float3 N = float3(0, 0, 1);
    
    float A = 0;
    float B = 0;
    const uint NumSamples = 1024;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, Roughness, N);
        float3 L = 2 * dot(V, H) * H - V;
        float NoL = saturate(L.z);
        float NoH = saturate(H.z);
        float VoH = saturate(dot(V, H));
        if (NoL > 0)
        {
            float G = G_Smith(Roughness, NoV, NoL);
            float G_Vis = G * VoH / (NoH * NoV);
            float Fc = pow(1 - VoH, 5);
            A += (1 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return float2(A, B) / NumSamples;
}

float4 main(VertexToPixel input) : SV_TARGET
{
    float roughness = input.uv.y;
    float nDotV = input.uv.x;
    
    float2 brdf = IntegrateBRDF(roughness, nDotV);
    return float4(brdf, 0, 1);
}