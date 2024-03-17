#include "Lighting.hlsli"
Texture2D AlbedoTexture : register(t0);
Texture2D NormalMap : register(t1);
Texture2D RoughnessMap : register(t2);
Texture2D MetalMap : register(t3);
SamplerState BasicSampler : register(s0);

cbuffer ExternalData : register(b0)
{
    float2 uvScale;
    float2 uvOffset;
    float3 cameraPosition;
    int lightCount;
    Light lights[MAX_LIGHTS];
}

// Struct representing the data we expect to receive from earlier pipeline stages
// - Should match the output of our corresponding vertex shader
// - The name of the struct itself is unimportant
// - The variable names don't have to match other shaders (just the semantics)
// - Each variable must have a semantic, which defines its usage
struct VertexToPixel
{
	// Data type
	//  |
	//  |   Name          Semantic
	//  |    |                |
	//  v    v                v
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
    float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPosition	: POSITIONT;
};

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// 
// - Input is the data coming down the pipeline (defined by the struct)
// - Output is a single color (float4)
// - Has a special semantic (SV_TARGET), which means 
//    "put the output of this into the current render target"
// - Named "main" because that's the default the shader compiler looks for
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
    input.normal = normalize(input.normal);
    input.tangent = normalize(input.tangent);
	
    input.uv = input.uv * uvScale +uvOffset;
	
    float4 surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv);
    surfaceColor.rgb = pow(surfaceColor.rgb, 2.2);
	
    input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);
	
    float roughness = RoughnessMap.Sample(BasicSampler, input.uv).r;
    float metal = MetalMap.Sample(BasicSampler, input.uv).r;
	
    float3 specColor = lerp(F0_NON_METAL.rrr, surfaceColor.rgb, metal);
	
    float3 totalLight = float3(0, 0, 0);
	
	// Loop and handle all lights
    for (int i = 0; i < lightCount; i++)
    {
		// Grab this light and normalize the direction (just in case)
        Light light = lights[i];
        light.Direction = normalize(light.Direction);

		// Run the correct lighting calculation based on the light's type
        switch (lights[i].Type)
        {
            case LIGHT_TYPE_DIRECTIONAL:
                totalLight += DirLightPBR(light, input.normal, input.worldPosition, cameraPosition, roughness, metal, surfaceColor.rgb, specColor);
                break;

            case LIGHT_TYPE_POINT:
                totalLight += PointLightPBR(light, input.normal, input.worldPosition, cameraPosition, roughness, metal, surfaceColor.rgb, specColor);
                break;

            case LIGHT_TYPE_SPOT:
                totalLight += SpotLightPBR(light, input.normal, input.worldPosition, cameraPosition, roughness, metal, surfaceColor.rgb, specColor);
                break;
        }
    }

	
	// Just return the input color
	// - This color (like most values passing through the rasterizer) is 
	//   interpolated for each pixel between the corresponding vertices 
	//   of the triangle we're rendering
    return float4(pow(totalLight, 1.0f / 2.2f), 1.0f);
    //return float4(specColor, 1.0f);
}