#include "Material.h"
#include "DX12Helper.h"

Material::Material(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState, DirectX::XMFLOAT3 colorTint, DirectX::XMFLOAT2 uvScale, DirectX::XMFLOAT2 uvOffset)
    : pipelineState(pipelineState), colorTint(colorTint), uvScale(uvScale), uvOffset(uvOffset)
{
    finalized = false;
    finalGPUHandleForSRVs = {};
}

DirectX::XMFLOAT3 Material::GetColorTint()
{
    return colorTint;
}

DirectX::XMFLOAT2 Material::GetUVScale()
{
    return uvScale;
}

DirectX::XMFLOAT2 Material::GetUVOffset()
{
    return uvOffset;
}

D3D12_GPU_DESCRIPTOR_HANDLE Material::GetFinalGPUHandleForTextures()
{
    return finalGPUHandleForSRVs;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> Material::GetPipelineState()
{
    return pipelineState;
}

void Material::SetColorTint()
{
    this->colorTint = colorTint;
}

void Material::SetUVScale()
{
    this->uvScale = uvScale;
}

void Material::SetUVOffset()
{
    this->uvOffset = uvOffset;
}

void Material::SetPipelineState()
{
    this->pipelineState = pipelineState;
}

void Material::AddTexture(D3D12_CPU_DESCRIPTOR_HANDLE srv, int slot)
{
    textureSRVsBySlot[slot] = srv;

}

void Material::FinalizeMaterial()
{
    if (finalized)
    {
        return;
    }

    DX12Helper& dx12Helper = DX12Helper::GetInstance();

    // In the future Change the 4 to the highest slot the texture can get to if the TextureSRVsbySlot is changed to hold more than 4 textures
    for (int i = 0; i < 4; i++)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = dx12Helper.CopySRVsToDescriptorHeapAndGetGPUDescriptorHandle(textureSRVsBySlot[i], 1);
        
        
        // make sure to save the D3D12_GPU_DESCRIPTOR_HANDLE to the first descriptor I copy (first iteration)
        if (i == 0)
        {
            finalGPUHandleForSRVs = handle;
        }

    }
    finalized = true;
}
