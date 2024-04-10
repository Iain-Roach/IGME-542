cbuffer externalData : register(b0)
{
    matrix view;
    matrix projection;

    float4 startColor;
    float4 endColor;

    float currentTime;
    float3 acceleration;

    float startSize;
    float endSize;
    float lifetime;
};

// Struct representing a single particle
// Note: the organization is due to 16-byte alignment!
struct Particle
{
    float EmitTime;
    float3 StartPosition;
    float3 StartVelocity;
};

StructuredBuffer<Particle> ParticleData : register(t0);

struct VertexToPixel
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 colorTint : COLOR;
};

VertexToPixel main(uint id : SV_VertexID)
{
    VertexToPixel output;

    uint particleID = id / 4; 
    uint cornerID = id % 4; 

    Particle p = ParticleData.Load(particleID);

    float age = currentTime - p.EmitTime;
    float agePercent = age / lifetime;

    float3 pos = acceleration * age * age / 2.0f + p.StartVelocity * age + p.StartPosition;

	// Size interpolation
    float size = lerp(startSize, endSize, agePercent);


    float2 offsets[4];
    offsets[0] = float2(-1.0f, +1.0f); // TL
    offsets[1] = float2(+1.0f, +1.0f); // TR
    offsets[2] = float2(+1.0f, -1.0f); // BR
    offsets[3] = float2(-1.0f, -1.0f); // BL


	// Billboarding!
    pos += float3(view._11, view._12, view._13) * offsets[cornerID].x;
    pos += float3(view._21, view._22, view._23) * offsets[cornerID].y;

    matrix viewProj = mul(projection, view);
    output.position = mul(viewProj, float4(pos, 1.0f));

    
    float2 uvs[4];
    uvs[0] = float2(0, 0);
    uvs[1] = float2(1, 0);
    uvs[2] = float2(1, 1);
    uvs[3] = float2(0, 1);
    output.uv = uvs[cornerID];
    output.colorTint = lerp(startColor, endColor, agePercent);

    return output;
}
	