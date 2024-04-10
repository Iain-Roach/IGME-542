
cbuffer externalData : register(b0)
{
    matrix view;
    matrix proj;
    
    float currentTime;
    
    float lifetime;
}

struct Particle
{
    float emitTime;
    float3 startPos;
   
    float3 startVel;
    float pad;
};

struct VertexToPixel
{
    float4 screenPosition : SV_POSITION;
    float2 uv : TEXCOORD;
};

StructuredBuffer<Particle> ParticleBuffer : register(t0);

VertexToPixel main(uint vertexID : SV_VertexID)
{
    VertexToPixel output;
    
//    Use the vertex’s id to calculate the particle index and corner index
//• Load (retrieve) the proper particle from the structured buffer
//• Simulation:
//o Calculate the age of the particle (in seconds and, potentially, as a percent of lifetime)
//o Use the age along with other particle data to calculate a final position
//o This will be extremely simplistic at this point!
//• Perform billboarding
//• Handle camera matrix transformations
//• Determine the proper UV for this vertex
    
    
    uint particleID = vertexID / 4;
    uint cornerID = vertexID % 4;
    Particle particle = ParticleBuffer.Load(particleID);
    
    float age = currentTime - particle.emitTime;
    float agePercentage = age / lifetime;
    
    //float3 pos = acceleration * age * age / 2.0 + particle.startVel * age + particle.startPos;
    
    float3 pos = particle.startPos + age * particle.startVel;
    
    // calculate all the other particle data
    
    
    
    
    
    
    
    
    
    
    
    
    
    // billboarding
    float2 offsets[4];
    offsets[0] = float2(-1.0f, +1.0f);
    offsets[1] = float2(+1.0f, +1.0f);
    offsets[2] = float2(+1.0f, -1.0f);
    offsets[3] = float2(-1.0f, -1.0f);
    
    pos += float3(view._11, view._12, view._13) * offsets[cornerID].x;
    pos += float3(view._21, view._21, view._21) * offsets[cornerID].y;
    
    
    matrix viewProj = mul(proj, view);
    output.screenPosition = mul(viewProj, float4(pos, 1.0f));
    
    float2 uvs[4];
    uvs[0] = float2(0, 0);
    uvs[1] = float2(1, 0);
    uvs[2] = float2(1, 1);
    uvs[3] = float2(0, 1);
    output.uv = uvs[cornerID];
    
	return output;
}