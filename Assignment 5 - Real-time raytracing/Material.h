#pragma once
#include <DirectXMath.h>
#include <wrl/client.h>
#include <d3d12.h>
class Material
{
public:
	Material(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState, DirectX::XMFLOAT3 colorTint, DirectX::XMFLOAT2 uvScale, DirectX::XMFLOAT2 uvOffset, float roughness = 1.0f, float metal = 0.0f);
	
	DirectX::XMFLOAT3 GetColorTint();
	DirectX::XMFLOAT2 GetUVScale();
	DirectX::XMFLOAT2 GetUVOffset();
	float GetMetal();
	float GetRoughness();

	D3D12_GPU_DESCRIPTOR_HANDLE GetFinalGPUHandleForTextures();
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState();

	void SetColorTint(DirectX::XMFLOAT3 tint);
	void SetUVScale(DirectX::XMFLOAT2 scale);
	void SetUVOffset(DirectX::XMFLOAT2 offset);
	void SetMetal(float metal);
	void SetRoughness(float roughness);

	void SetPipelineState();

	void AddTexture(D3D12_CPU_DESCRIPTOR_HANDLE srv, int slot);
	void FinalizeMaterial();



private:
	DirectX::XMFLOAT3 colorTint;
	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT2 uvOffset;
	float roughness;
	float metal;
	bool finalized;
	
	D3D12_GPU_DESCRIPTOR_HANDLE finalGPUHandleForSRVs;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState; // replaces individual pixel and vertex shaders
	D3D12_CPU_DESCRIPTOR_HANDLE textureSRVsBySlot[4];  // For more flexibility change to hold 128 textures, however would need to track
													   // the highest slot (index) that is actually used in the array.
	
};

