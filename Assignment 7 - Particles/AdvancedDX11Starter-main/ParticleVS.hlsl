
cbuffer externalData : register(b0)
{
    matrix world;
    matrix view;
    matrix proj;
}

struct VertexShaderInput
{
    float3 localPosition : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct VertexToPixel
{
    float4 screenPosition : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

VertexToPixel main( VertexShaderInput input )
{
    VertexToPixel output;
    
    matrix wvp = mul(proj, mul(view, world));
    output.screenPosition = mul(wvp, float4(input.localPosition, 1.0f));
    
    output.uv = input.uv;
    output.color = input.color;
    
	return output;
}