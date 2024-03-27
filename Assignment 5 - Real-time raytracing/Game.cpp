#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "DX12Helper.h"
#include "BufferStruct.h"
#include "RaytracingHelper.h"

// Needed for a helper function to load pre-compiled shader files
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

#include <time.h>
#include <stdlib.h>

// For the DirectX Math library
using namespace DirectX;

#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// Direct3D itself, and our window, are not ready at this point!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,			// The application's handle
		L"DirectX Game",	// Text for the window's title bar (as a wide-character string)
		1280,				// Width of the window's client area
		720,				// Height of the window's client area
		false,				// Sync the framerate to the monitor refresh? (lock framerate)
		true), 	// Show extra stats (fps) in title bar?
		lightCount(32)
{
	ibView = {};
	vbView = {};
#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif
}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Delete all objects manually created within this class
//  - Release() all Direct3D objects created within this class
// --------------------------------------------------------
Game::~Game()
{
	// Call delete or delete[] on any objects or arrays you've
	// created using new or new[] within this class
	// - Note: this is unnecessary if using smart pointers
	

	// Call Release() on any Direct3D objects made within this class
	// - Note: this is unnecessary for D3D objects stored in ComPtrs
	// We need to wait here until the GPU
	// is actually done with its work
	DX12Helper::GetInstance().WaitForGPU();

	delete& RaytracingHelper::GetInstance();
}

// --------------------------------------------------------
// Called once per program, after Direct3D and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Attempt to initialize DXR
	RaytracingHelper::GetInstance().Initialize(
		windowWidth,
		windowHeight,
		device,
		commandQueue,
		commandList,
		FixPath(L"Raytracing.cso"));

	srand((unsigned int)time(0));


	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	// - You'll be expanding and/or replacing these later
	CreateRootSigAndPipelineState();
	CreateBasicGeometry();
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dir1.Intensity = 1.0f;

	Light point = {};
	point.Type = LIGHT_TYPE_POINT;
	point.Direction = XMFLOAT3(0, 0, 0);
	point.Color = XMFLOAT3(0.5f, 0.2f, 0.5f);
	point.Range = 10.0f;
	point.Intensity = 3.0f;

	// Add light to the list
	lights.push_back(dir1);
	lights.push_back(point);

	

	// Make sure we're exactly MAX_LIGHTS big
	lights.resize(MAX_LIGHTS);



	// initialize camera
	camera = std::make_shared<Camera>(XMFLOAT3(0.0f, -6.0f, -10.0f), 5.0f, 0.002f, XM_PIDIV4, windowWidth / (float)windowHeight);
}

// --------------------------------------------------------
// Loads the two basic shaders, then creates the root signature
// and pipeline state object for our very basic demo.
// --------------------------------------------------------
void Game::CreateRootSigAndPipelineState()
{
	// Blobs to hold raw shader byte code used in several steps below
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderByteCode;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderByteCode;
	// Load shaders
	{
		// Read our compiled vertex shader code into a blob
		// - Essentially just "open the file and plop its contents here"
		D3DReadFileToBlob(FixPath(L"VertexShader.cso").c_str(),
			vertexShaderByteCode.GetAddressOf());
		D3DReadFileToBlob(FixPath(L"PixelShader.cso").c_str(),
			pixelShaderByteCode.GetAddressOf());
	}
	// Input layout
	const unsigned int inputElementCount = 4;
	D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount] = {};
	{
		inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT; // R32 G32 B32 = float3
		inputElements[0].SemanticName = "POSITION"; // Name must match semantic in shader
		inputElements[0].SemanticIndex = 0; // This is the first POSITION semantic
		inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT; // R32 G32 = float2
		inputElements[1].SemanticName = "TEXCOORD";
		inputElements[1].SemanticIndex = 0; // This is the first TEXCOORD semantic
		inputElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT; // R32 G32 B32 = float3
		inputElements[2].SemanticName = "NORMAL";
		inputElements[2].SemanticIndex = 0; // This is the first NORMAL semantic
		inputElements[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		inputElements[3].Format = DXGI_FORMAT_R32G32B32_FLOAT; // R32 G32 B32 = float3
		inputElements[3].SemanticName = "TANGENT";
		inputElements[3].SemanticIndex = 0; // This is the first TANGENT semantic
	}
	// Root Signature
	{
		//// Describe and serialize the root signature
		//D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		//rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		//rootSig.NumParameters = 0;
		//rootSig.pParameters = 0;
		//rootSig.NumStaticSamplers = 0;
		//rootSig.pStaticSamplers = 0;
		//ID3DBlob* serializedRootSig = 0;
		//ID3DBlob* errors = 0;
		//D3D12SerializeRootSignature(
		//	&rootSig,
		//	D3D_ROOT_SIGNATURE_VERSION_1,
		//	&serializedRootSig,
		//	&errors);
		//// Check for errors during serialization
		//if (errors != 0)
		//{
		//	OutputDebugString((wchar_t*)errors->GetBufferPointer());
		//}
		//// Actually create the root sig
		//device->CreateRootSignature(
		//	0,
		//	serializedRootSig->GetBufferPointer(),
		//	serializedRootSig->GetBufferSize(),
		//	IID_PPV_ARGS(rootSignature.GetAddressOf()));

		//// Define a table of CBV's (constant buffer views)
		//D3D12_DESCRIPTOR_RANGE cbvTable = {};
		//cbvTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		//cbvTable.NumDescriptors = 1;
		//cbvTable.BaseShaderRegister = 0;
		//cbvTable.RegisterSpace = 0;
		//cbvTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		//// Define the root parameter
		//D3D12_ROOT_PARAMETER rootParam = {};
		//rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		//rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		//rootParam.DescriptorTable.NumDescriptorRanges = 1;
		//rootParam.DescriptorTable.pDescriptorRanges = &cbvTable;
		//// Describe the overall the root signature
		//D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		//rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		//rootSig.NumParameters = 1;
		//rootSig.pParameters = &rootParam;
		//rootSig.NumStaticSamplers = 0;
		//rootSig.pStaticSamplers = 0;

		// Describe the range of CBVs needed for the vertex shader
		D3D12_DESCRIPTOR_RANGE cbvRangeVS = {};
		cbvRangeVS.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangeVS.NumDescriptors = 1;
		cbvRangeVS.BaseShaderRegister = 0;
		cbvRangeVS.RegisterSpace = 0;
		cbvRangeVS.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		// Describe the range of CBVs needed for the pixel shader
		D3D12_DESCRIPTOR_RANGE cbvRangePS = {};
		cbvRangePS.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvRangePS.NumDescriptors = 1;
		cbvRangePS.BaseShaderRegister = 0;
		cbvRangePS.RegisterSpace = 0;
		cbvRangePS.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		// Create a range of SRV's for textures
		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 4; // Set to max number of textures at once (match pixel shader!)
		srvRange.BaseShaderRegister = 0; // Starts at s0 (match pixel shader!)
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		// Create the root parameters
		D3D12_ROOT_PARAMETER rootParams[3] = {};
		// CBV table param for vertex shader
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[0].DescriptorTable.pDescriptorRanges = &cbvRangeVS;
		// CBV table param for pixel shader
		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[1].DescriptorTable.pDescriptorRanges = &cbvRangePS;
		// SRV table param
		rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[2].DescriptorTable.pDescriptorRanges = &srvRange;
		// Create a single static sampler (available to all pixel shaders at the same slot)
		D3D12_STATIC_SAMPLER_DESC anisoWrap = {};
		anisoWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.Filter = D3D12_FILTER_ANISOTROPIC;
		anisoWrap.MaxAnisotropy = 16;
		anisoWrap.MaxLOD = D3D12_FLOAT32_MAX;
		anisoWrap.ShaderRegister = 0; // register(s0)
		anisoWrap.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		D3D12_STATIC_SAMPLER_DESC samplers[] = { anisoWrap };
		// Describe and serialize the root signature
		D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSig.NumParameters = ARRAYSIZE(rootParams);
		rootSig.pParameters = rootParams;
		rootSig.NumStaticSamplers = ARRAYSIZE(samplers);
		rootSig.pStaticSamplers = samplers;

		ID3DBlob* serializedRootSig = 0;
		ID3DBlob* errors = 0;

		D3D12SerializeRootSignature(
			&rootSig,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&serializedRootSig,
			&errors);

		// Check for errors during serialization
		if (errors != 0)
		{
			OutputDebugString((wchar_t*)errors->GetBufferPointer());
		}

		// Actually create the root sig
		device->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(rootSignature.GetAddressOf()));
	}
	// Pipeline state
	{
		//// Describe the pipeline state
		//D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		//// -- Input assembler related ---
		//psoDesc.InputLayout.NumElements = inputElementCount;
		//psoDesc.InputLayout.pInputElementDescs = inputElements;
		//psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		//// Root sig
		//psoDesc.pRootSignature = rootSignature.Get();
		//// -- Shaders (VS/PS) ---
		//psoDesc.VS.pShaderBytecode = vertexShaderByteCode->GetBufferPointer();
		//psoDesc.VS.BytecodeLength = vertexShaderByteCode->GetBufferSize();
		//psoDesc.PS.pShaderBytecode = pixelShaderByteCode->GetBufferPointer();
		//psoDesc.PS.BytecodeLength = pixelShaderByteCode->GetBufferSize();
		//// -- Render targets ---
		//psoDesc.NumRenderTargets = 1;
		//psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		//psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		//psoDesc.SampleDesc.Count = 1;
		//psoDesc.SampleDesc.Quality = 0;
		//// -- States ---
		//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		//psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		//psoDesc.RasterizerState.DepthClipEnable = true;
		//psoDesc.DepthStencilState.DepthEnable = true;
		//psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		//psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		//psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		//psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		//psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		//psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask =
		//	D3D12_COLOR_WRITE_ENABLE_ALL;
		//// -- Misc ---
		//psoDesc.SampleMask = 0xffffffff;
		//// Create the pipe state object
		//device->CreateGraphicsPipelineState(&psoDesc,
		//	IID_PPV_ARGS(pipelineState.GetAddressOf()));
		// Describe the pipeline state
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

		// -- Input assembler related ---
		psoDesc.InputLayout.NumElements = inputElementCount;
		psoDesc.InputLayout.pInputElementDescs = inputElements;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		// Overall primitive topology type (triangle, line, etc.) is set here 
		// IASetPrimTop() is still used to set list/strip/adj options
		// See: https://docs.microsoft.com/en-us/windows/desktop/direct3d12/managing-graphics-pipeline-state-in-direct3d-12

		// Root sig
		psoDesc.pRootSignature = rootSignature.Get();

		// -- Shaders (VS/PS) --- 
		psoDesc.VS.pShaderBytecode = vertexShaderByteCode->GetBufferPointer();
		psoDesc.VS.BytecodeLength = vertexShaderByteCode->GetBufferSize();
		psoDesc.PS.pShaderBytecode = pixelShaderByteCode->GetBufferPointer();
		psoDesc.PS.BytecodeLength = pixelShaderByteCode->GetBufferSize();

		// -- Render targets ---
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		// -- States ---
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		psoDesc.RasterizerState.DepthClipEnable = true;

		psoDesc.DepthStencilState.DepthEnable = true;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		// -- Misc ---
		psoDesc.SampleMask = 0xffffffff;

		// Create the pipe state object
		device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
	}
}


// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	// Textures
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneAlbedo = DX12Helper::GetInstance().LoadTexture(FixPath(L"../../Assets/Textures/cobblestone_albedo.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneNormals = DX12Helper::GetInstance().LoadTexture(FixPath(L"../../Assets/Textures/cobblestone_normals.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneRoughness = DX12Helper::GetInstance().LoadTexture(FixPath(L"../../Assets/Textures/cobblestone_roughness.png").c_str());
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneMetal = DX12Helper::GetInstance().LoadTexture(FixPath(L"../../Assets/Textures/cobblestone_metal.png").c_str());

	// Materials
	std::shared_ptr<Material> cobblestoneMaterial = std::make_shared<Material>(pipelineState, XMFLOAT3(.6, .6, .6), XMFLOAT2(1, 1), XMFLOAT2(0, 0), .5f, .5f);
	cobblestoneMaterial->AddTexture(cobblestoneAlbedo, 0);
	cobblestoneMaterial->AddTexture(cobblestoneNormals, 1);
	cobblestoneMaterial->AddTexture(cobblestoneRoughness, 2);
	cobblestoneMaterial->AddTexture(cobblestoneMetal, 3);
	cobblestoneMaterial->FinalizeMaterial();

	std::shared_ptr<Material> floorMaterial = std::make_shared<Material>(pipelineState, XMFLOAT3(.3, .3, .3), XMFLOAT2(1, 1), XMFLOAT2(0, 0));


	std::shared_ptr<Mesh> cube = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/cube.obj").c_str());
	std::shared_ptr<Mesh> helix = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/helix.obj").c_str());
	std::shared_ptr<Mesh> torus = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/torus.obj").c_str());
	std::shared_ptr<Mesh> sphere = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/sphere.obj").c_str());

	std::shared_ptr<Entity> eCube = std::make_shared<Entity>(cube, cobblestoneMaterial);
	eCube->GetTransform()->SetPosition(-4, -5.5, 0);

	std::shared_ptr<Entity> eHelix = std::make_shared<Entity>(helix, cobblestoneMaterial);
	eHelix->GetTransform()->SetPosition(4, -6, 0);

	std::shared_ptr<Entity> eTorus = std::make_shared<Entity>(torus, cobblestoneMaterial);
	eTorus->GetTransform()->SetPosition(0, -5.8, 0);

	std::shared_ptr<Entity> floor = std::make_shared<Entity>(cube, floorMaterial);
	floor->GetTransform()->SetPosition(0, -8, 0);
	floor->GetTransform()->SetScale(30.0f, 1.0f, 30.0f);

	

	entities.push_back(floor);
	entities.push_back(eHelix);
	entities.push_back(eTorus);
	
	entities.push_back(eCube);
	
	

	// Spheres
	for (int i = 0; i < 5; i++)
	{

		std::shared_ptr<Material> mat = std::make_shared<Material>(pipelineState, XMFLOAT3(RandomRange(0.0f, 1.0f), RandomRange(0.0f, 1.0f), RandomRange(0.0f, 1.0f)), XMFLOAT2(1, 1), XMFLOAT2(0, 0));
		std::shared_ptr<Entity> eSphere = std::make_shared<Entity>(sphere, mat);
		eSphere->GetTransform()->SetPosition(RandomRange(-8.0f, 9.0f), RandomRange(-5.0f, 0.0f), RandomRange(-9.0f, 9.0f));
		float sphereScalar = RandomRange(0.5f, 2.0f);
		eSphere->GetTransform()->SetScale(sphereScalar, sphereScalar, sphereScalar);

		entities.push_back(eSphere);
	}

	RaytracingHelper::GetInstance().CreateTopLevelAccelerationStructureForScene(entities);
}


// --------------------------------------------------------
// Handle resizing to match the new window size.
//  - DXCore needs to resize the back buffer
//  - Eventually, we'll want to update our 3D camera
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Resize raytracing output texture
	RaytracingHelper::GetInstance().ResizeOutputUAV(windowWidth, windowHeight);

	// update camera projection
	if (camera)
	{
		camera->UpdateProjectionMatrix((float)windowWidth / windowHeight);
	}
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Example input checking: Quit if the escape key is pressed
	if (Input::GetInstance().KeyDown(VK_ESCAPE))
		Quit();

	// Update entities so they spin
	for (int i = 0; i < entities.size(); i++)
	{
		if(i != 0)
		entities[i]->GetTransform()->Rotate(0, deltaTime, 0);
		if (i == 2)
		{
			entities[i]->GetTransform()->Rotate(deltaTime, 0, 0);
		}

		if (i >= 4)
		{
			entities[i]->GetTransform()->MoveRelative(XMFLOAT3(sin(deltaTime + i) * .1f, 0.0, sin(deltaTime + i) * .000000001f));
		}
	}




	camera->Update(deltaTime);
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	//// Grab the current back buffer for this frame
	//Microsoft::WRL::ComPtr<ID3D12Resource> currentBackBuffer = backBuffers[currentSwapBuffer];
	//// Clearing the render target
	//{
	//	// Transition the back buffer from present to render target
	//	D3D12_RESOURCE_BARRIER rb = {};
	//	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//	rb.Transition.pResource = currentBackBuffer.Get();
	//	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	//	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	//	commandList->ResourceBarrier(1, &rb);
	//	// Background color (Cornflower Blue in this case) for clearing
	//	float color[] = { 0.4f, 0.6f, 0.75f, 1.0f };
	//	// Clear the RTV
	//	commandList->ClearRenderTargetView(
	//		rtvHandles[currentSwapBuffer],
	//		color,
	//		0, 0); // No scissor rectangles
	//	// Clear the depth buffer, too
	//	commandList->ClearDepthStencilView(
	//		dsvHandle,
	//		D3D12_CLEAR_FLAG_DEPTH,
	//		1.0f, // Max depth = 1.0f
	//		0, // Not clearing stencil, but need a value
	//		0, 0); // No scissor rects
	//}

	DX12Helper& dx12Helper = DX12Helper::GetInstance();

	// Reset commnand allocator
	commandAllocator->Reset();

	

	// Reset command list
	commandList->Reset(commandAllocator.Get(), 0);

	// RaytracingHelper Calls
	// Update raytracing accel structure
	RaytracingHelper::GetInstance().CreateTopLevelAccelerationStructureForScene(entities);

	
	// Perform raytrace, including execution of command list
	RaytracingHelper::GetInstance().Raytrace(camera, backBuffers[currentSwapBuffer]);

	// swapChain->Present same as before
	bool vsyncNecessary = vsync || !deviceSupportsTearing || isFullscreen;
	swapChain->Present(
		vsyncNecessary ? 1 : 0,
		vsyncNecessary ? 0 : DXGI_PRESENT_ALLOW_TEARING);

	// Update current swap buffer index same as before

	currentSwapBuffer++;
	if (currentSwapBuffer >= numBackBuffers)
		currentSwapBuffer = 0;

	// Fix sync issue?
	/*currentSwapBuffer = dx12Helper.SyncSwapChain(currentSwapBuffer);*/


	//// Rendering here!
	//{
	//	DX12Helper& dx12Helper = DX12Helper::GetInstance();

	//	// Set overall pipeline state
	//	// commandList->SetPipelineState(pipelineState.Get());
	//	// Root sig (must happen before root descriptor table)
	//	commandList->SetGraphicsRootSignature(rootSignature.Get());

	//	// ConstantBufferDescriptorHeap
	//	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap =dx12Helper.GetCBVSRVDescriptorHeap();
	//	commandList->SetDescriptorHeaps(1, descriptorHeap.GetAddressOf());


	//	// Set up other commands for rendering
	//	commandList->OMSetRenderTargets(1, &rtvHandles[currentSwapBuffer], true, &dsvHandle);
	//	commandList->RSSetViewports(1, &viewport);
	//	commandList->RSSetScissorRects(1, &scissorRect);

	//	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//	// Loop to render all entities
	//	for (int i = 0; i < entities.size(); i++)
	//	{
	//		std::shared_ptr<Material> mat = entities[i]->GetMaterial();

	//		commandList->SetPipelineState(mat->GetPipelineState().Get());

	//		// fill out VertexShaderExternalData struct
	//		VertexShaderExternalData vsData = {};
	//		vsData.world = entities[i]->GetTransform()->GetWorldMatrix();
	//		vsData.view = camera->GetView();
	//		vsData.projection = camera->GetProjection();
	//		vsData.worldInverseTranspose = entities[i]->GetTransform()->GetWorldInverseTransposeMatrix();

	//		// copy struct to the gpu and get corresponding handle to the constant buffer view
	//		D3D12_GPU_DESCRIPTOR_HANDLE cbvHandle = dx12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle((void*)(&vsData), sizeof(VertexShaderExternalData));

	//		// Set handle
	//		commandList->SetGraphicsRootDescriptorTable(0, cbvHandle);

	//		// Pixel shader data and cbuffer setup
	//		{
	//			PixelShaderExternalData psData = {};
	//			psData.uvScale = mat->GetUVScale();
	//			psData.uvOffset = mat->GetUVOffset();
	//			psData.cameraPosition = camera->GetTransform()->GetPosition();
	//			psData.lightCount = lightCount;
	//			memcpy(psData.lights, &lights[0], sizeof(Light) * MAX_LIGHTS);
	//			// Send this to a chunk of the constant buffer heap
	//			// and grab the GPU handle for it so we can set it for this draw
	//				D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS =
	//				dx12Helper.FillNextConstantBufferAndGetGPUDescriptorHandle(
	//					(void*)(&psData), sizeof(PixelShaderExternalData));
	//			// Set this constant buffer handle
	//			// Note: This assumes that descriptor table 1 is the
	//			// place to put this particular descriptor. This
	//			// is based on how we set up our root signature.
	//			commandList->SetGraphicsRootDescriptorTable(1, cbHandlePS);
	//		}



	//		// Set the SRV descriptor handle for this material's textures
	//		// Note: This assumes that descriptor table 2 is for textures (as per our rootsig)
	//		commandList->SetGraphicsRootDescriptorTable(2, mat->GetFinalGPUHandleForTextures());

	//		// grab vertex buffer view and index buffer view from this entity's mesh
	//		std::shared_ptr<Mesh> mesh = entities[i]->GetMesh();
	//		D3D12_VERTEX_BUFFER_VIEW vbv = mesh->GetVBView();
	//		D3D12_INDEX_BUFFER_VIEW ibv = mesh->GetIBView();

	//		// Set using IASETVertexBuffer and IASetIndexBuffer
	//		commandList->IASetVertexBuffers(0, 1, &vbv);
	//		commandList->IASetIndexBuffer(&ibv);

	//		// Draw
	//		commandList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
	//	}

	//}

	//// Present
	//{
	//	// Transition back to present
	//	D3D12_RESOURCE_BARRIER rb = {};
	//	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//	rb.Transition.pResource = currentBackBuffer.Get();
	//	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	//	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	//	commandList->ResourceBarrier(1, &rb);
	//	// Must occur BEFORE present
	//	DX12Helper::GetInstance().CloseExecuteAndResetCommandList();
	//	// Present the current back buffer
	//	bool vsyncNecessary = vsync || !deviceSupportsTearing || isFullscreen;
	//	swapChain->Present(
	//		vsyncNecessary ? 1 : 0,
	//		vsyncNecessary ? 0 : DXGI_PRESENT_ALLOW_TEARING);
	//	// Figure out which buffer is next
	//	currentSwapBuffer++;
	//	if (currentSwapBuffer >= numBackBuffers)
	//		currentSwapBuffer = 0;
	//}
DX12Helper::GetInstance().WaitForGPU();
}