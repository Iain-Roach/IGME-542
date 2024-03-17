#pragma once

#include <wrl/client.h>
#include <string>

#include "Vertex.h"
#include <d3d12.h>

struct MeshRaytracingData
{
	D3D12_GPU_DESCRIPTOR_HANDLE IndexbufferSRV{ };
	D3D12_GPU_DESCRIPTOR_HANDLE VertexBufferSRV{ };
	Microsoft::WRL::ComPtr<ID3D12Resource> BLAS;
	unsigned int HitGroupIndex = 0;
};

class Mesh
{
	

public:
	Mesh(Vertex* vertArray, size_t numVerts, unsigned int* indexArray, size_t numIndices);
	Mesh(const std::wstring& objFile);
	~Mesh();

	

	// Getters for mesh data
	unsigned int GetIndexCount();
	unsigned int GetVertexCount();
	D3D12_VERTEX_BUFFER_VIEW GetVBView() { return vbView; } // Renamed
	D3D12_INDEX_BUFFER_VIEW GetIBView() { return ibView; } // Renamed

	Microsoft::WRL::ComPtr<ID3D12Resource> GetVBResource() { return vertexBuffer; }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetIBResource() { return indexBuffer; }

	MeshRaytracingData GetRaytracingData() { return raytracingData; }

	


private:
	// D3D buffers
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;

	// Total indices in this mesh
	unsigned int numIndices;

	unsigned int numVertices;

	// Helper for creating buffers (in the event we add more constructor overloads)
	void CreateBuffers(Vertex* vertArray, size_t numVerts, unsigned int* indexArray, size_t numIndices);
	void CalculateTangents(Vertex* verts, size_t numVerts, unsigned int* indices, size_t numIndices);

	D3D12_VERTEX_BUFFER_VIEW vbView;
	D3D12_INDEX_BUFFER_VIEW ibView;

	MeshRaytracingData raytracingData;
};