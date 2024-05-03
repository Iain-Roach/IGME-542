
#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)

#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "Helpers.h"

#include "WICTextureLoader.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"


// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// Helper macros for making texture and shader loading code more succinct
#define LoadTexture(file, srv) CreateWICTextureFromFile(device.Get(), context.Get(), FixPath(file).c_str(), 0, srv.GetAddressOf())
#define LoadShader(type, file) std::make_shared<type>(device.Get(), context.Get(), FixPath(file).c_str())


// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
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
		true),				// Show extra stats (fps) in title bar?
	camera(0),
	sky(0),
	lightCount(0),
	showUIDemoWindow(false),
	showPointLights(false)
{
	ssaoSamples = 64;
	ssaoRadius = 1.0f;
	// Seed random
	srand((unsigned int)time(0));

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif

}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those DirectX objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each DirectX object

	// ImGui clean up
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Initialize ImGui itself & platform/renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device.Get(), context.Get());
	ImGui::StyleColorsDark();

	// Asset loading and entity creation
	LoadAssetsAndCreateEntities();
	
	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set up lights initially
	lightCount = 1;
	GenerateLights();

	// Set initial graphics API state
	//  - These settings persist until we change them
	{
		// Tell the input assembler (IA) stage of the pipeline what kind of
		// geometric primitives (points, lines or triangles) we want to draw.  
		// Essentially: "What kind of shape should the GPU draw with our vertices?"
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// Create the camera
	camera = std::make_shared<Camera>(
		0.0f, 0.0f, -15.0f,	// Position
		5.0f,				// Move speed (world units)
		0.002f,				// Look speed (cursor movement pixels --> radians for rotation)
		XM_PIDIV4,			// Field of view
		(float)windowWidth / windowHeight,  // Aspect ratio
		0.01f,				// Near clip
		100.0f,				// Far clip
		CameraProjectionType::Perspective);

	D3D11_SAMPLER_DESC clampDesc = {};
	clampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	clampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	clampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	clampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	clampDesc.MaxAnisotropy = 16;
	clampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&clampDesc, clampSamplerOptions.GetAddressOf());


	Microsoft::WRL::ComPtr<ID3D11Texture2D> rtTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneColorsTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ambientTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ssaoTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ssaoBlurTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = windowWidth;
	texDesc.Height = windowHeight;
	texDesc.ArraySize = 1;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Need both!
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Might occasionally use other formats
	texDesc.MipLevels = 1; // Usually no mip chain needed for render targets
	texDesc.MiscFlags = 0;
	texDesc.SampleDesc.Count = 1; // Can't be zero
	texDesc.SampleDesc.Quality = 0;
	device->CreateTexture2D(&texDesc, 0, rtTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture
	device->CreateTexture2D(&texDesc, 0, sceneColorsTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture
	device->CreateTexture2D(&texDesc, 0, ambientTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture
	device->CreateTexture2D(&texDesc, 0, ssaoTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture
	device->CreateTexture2D(&texDesc, 0, ssaoBlurTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture
	// device->CreateTexture2D(&texDesc, 0, depthTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture

	//texDesc.Format = DXGI_FORMAT_R32_FLOAT;
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = windowWidth;
	depthDesc.Height = windowHeight;
	depthDesc.ArraySize = 1;
	depthDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Need both!
	depthDesc.Format = DXGI_FORMAT_R32_FLOAT; // Might occasionally use other formats
	depthDesc.MipLevels = 1; // Usually no mip chain needed for render targets
	depthDesc.MiscFlags = 0;
	depthDesc.SampleDesc.Count = 1; // Can't be zero
	depthDesc.SampleDesc.Quality = 0;
	device->CreateTexture2D(&depthDesc, 0, depthTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // This points to a Texture2D
	rtvDesc.Texture2D.MipSlice = 0; // Which mip are we rendering into?
	rtvDesc.Format = texDesc.Format; // Same format as texture

	device->CreateRenderTargetView(rtTexture.Get(), &rtvDesc, sceneNormalsRTV.GetAddressOf());
	device->CreateShaderResourceView(rtTexture.Get(), 0, sceneNormalsSRV.GetAddressOf());

	device->CreateRenderTargetView(sceneColorsTexture.Get(), &rtvDesc, sceneColorsRTV.GetAddressOf());
	device->CreateShaderResourceView(sceneColorsTexture.Get(), 0, sceneColorsSRV.GetAddressOf());

	device->CreateRenderTargetView(ambientTexture.Get(), &rtvDesc, sceneAmbientRTV.GetAddressOf());
	device->CreateShaderResourceView(ambientTexture.Get(), 0, sceneAmbientSRV.GetAddressOf());

	device->CreateRenderTargetView(ssaoTexture.Get(), &rtvDesc, ssaoRTV.GetAddressOf());
	device->CreateShaderResourceView(ssaoTexture.Get(), 0, ssaoSRV.GetAddressOf());

	device->CreateRenderTargetView(ssaoBlurTexture.Get(), &rtvDesc, blurRTV.GetAddressOf());
	device->CreateShaderResourceView(ssaoBlurTexture.Get(), 0, blurSRV.GetAddressOf());

	D3D11_RENDER_TARGET_VIEW_DESC rtvDepthDesc = {};
	rtvDepthDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // This points to a Texture2D
	rtvDepthDesc.Texture2D.MipSlice = 0; // Which mip are we rendering into?
	rtvDepthDesc.Format = depthDesc.Format; // Same format as texture

	device->CreateRenderTargetView(depthTexture.Get(), &rtvDepthDesc, depthRTV.GetAddressOf());
	device->CreateShaderResourceView(depthTexture.Get(), 0, depthSRV.GetAddressOf());


	// Random SSAO Texture
	const int textureSize = 4;
	const int totalPixels = textureSize * textureSize;

	XMFLOAT4 randomPixels[totalPixels] = {};
	for (int i = 0; i < totalPixels; i++)
	{
		XMVECTOR randomVec = XMVectorSet(RandomRange(-1, 1), RandomRange(-1, 1), 0, 0);
		XMStoreFloat4(&randomPixels[i], XMVector3Normalize(randomVec));
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> ssaoTex;
	D3D11_TEXTURE2D_DESC tDesc = {};
	tDesc.Width = textureSize;
	tDesc.Height = textureSize;
	tDesc.ArraySize = 1;
	tDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	tDesc.MipLevels = 1;
	tDesc.SampleDesc.Count = 1;

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = randomPixels;
	data.SysMemPitch = sizeof(float) * 4 * textureSize;

	device->CreateTexture2D(&tDesc, &data, ssaoTex.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = tDesc.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	device->CreateShaderResourceView(ssaoTex.Get(), &srvDesc, randomTexSRV.GetAddressOf());
	

	// Want to save above texture and srv to game.h probably


	// Set up the array of ssao offset vectors (count must match shader!)
	for (int i = 0; i < 64; i++)
	{
		// Offsets should be in hemisphere ([-1,1], [-1,1], [0,1])
		// Note: Must be stored as float4’s due to cbuffer data packing!
		ssaoOffsets[i] = XMFLOAT4(
			(float)rand() / RAND_MAX * 2 - 1, // -1 to 1
			(float)rand() / RAND_MAX * 2 - 1, // -1 to 1
			(float)rand() / RAND_MAX, // 0 to 1
			0);
		XMVECTOR offset = XMVector3Normalize(XMLoadFloat4(&ssaoOffsets[i]));
		// Scale over the array, such that more of the values are
		// closer to the minimum than the maximum (see image -->)
		float scale = (float)i / 64;
		XMVECTOR acceleratedScale = XMVectorLerp(
			XMVectorSet(0.1f, 0.1f, 0.1f, 1),
			XMVectorSet(1, 1, 1, 1),
			scale * scale);
		XMStoreFloat4(&ssaoOffsets[i], offset * acceleratedScale);
	}


}


// --------------------------------------------------------
// Load all assets and create materials, entities, etc.
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Load shaders using our succinct LoadShader() macro
	std::shared_ptr<SimpleVertexShader> vertexShader	= LoadShader(SimpleVertexShader, L"VertexShader.cso");
	std::shared_ptr<SimplePixelShader> pixelShader		= LoadShader(SimplePixelShader, L"PixelShader.cso");
	std::shared_ptr<SimplePixelShader> pixelShaderPBR	= LoadShader(SimplePixelShader, L"PixelShaderPBR.cso");
	solidColorPS = LoadShader(SimplePixelShader, L"SolidColorPS.cso");
	simpleTexturePS = LoadShader(SimplePixelShader, L"SimpleTexturePS.cso");
	
	std::shared_ptr<SimpleVertexShader> skyVS = LoadShader(SimpleVertexShader, L"SkyVS.cso");
	std::shared_ptr<SimplePixelShader> skyPS  = LoadShader(SimplePixelShader, L"SkyPS.cso");

	fullscreenVS = LoadShader(SimpleVertexShader, L"FullscreenVS.cso");
	ssaoPS = LoadShader(SimplePixelShader, L"SSAOPS.cso");
	blurPS = LoadShader(SimplePixelShader, L"BlurSSAOPS.cso");
	combinePS = LoadShader(SimplePixelShader, L"CombineSSAOPS.cso");

	std::shared_ptr<SimplePixelShader> specConvPS = LoadShader(SimplePixelShader, L"SpecularConvolution.cso");
	std::shared_ptr<SimplePixelShader> brdfPS = LoadShader(SimplePixelShader, L"BrdfLookUpTablePS.cso");
	std::shared_ptr<SimplePixelShader> irrPS = LoadShader(SimplePixelShader, L"IrradianceMapPS.cso");


	// Make the meshes
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/sphere.obj").c_str(), device);
	std::shared_ptr<Mesh> helixMesh = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/helix.obj").c_str(), device);
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/cube.obj").c_str(), device);
	std::shared_ptr<Mesh> coneMesh = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/cone.obj").c_str(), device);
	
	// Declare the textures we'll need
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobbleA,  cobbleN,  cobbleR,  cobbleM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorA,  floorN,  floorR,  floorM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> paintA,  paintN,  paintR,  paintM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scratchedA,  scratchedN,  scratchedR,  scratchedM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bronzeA,  bronzeN,  bronzeR,  bronzeM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughA,  roughN,  roughR,  roughM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodA,  woodN,  woodR,  woodM;

	// Load the textures using our succinct LoadTexture() macro
	LoadTexture(L"../../Assets/Textures/cobblestone_albedo.png", cobbleA);
	LoadTexture(L"../../Assets/Textures/cobblestone_normals.png", cobbleN);
	LoadTexture(L"../../Assets/Textures/cobblestone_roughness.png", cobbleR);
	LoadTexture(L"../../Assets/Textures/cobblestone_metal.png", cobbleM);

	LoadTexture(L"../../Assets/Textures/floor_albedo.png", floorA);
	LoadTexture(L"../../Assets/Textures/floor_normals.png", floorN);
	LoadTexture(L"../../Assets/Textures/floor_roughness.png", floorR);
	LoadTexture(L"../../Assets/Textures/floor_metal.png", floorM);
	
	LoadTexture(L"../../Assets/Textures/paint_albedo.png", paintA);
	LoadTexture(L"../../Assets/Textures/paint_normals.png", paintN);
	LoadTexture(L"../../Assets/Textures/paint_roughness.png", paintR);
	LoadTexture(L"../../Assets/Textures/paint_metal.png", paintM);
	
	LoadTexture(L"../../Assets/Textures/scratched_albedo.png", scratchedA);
	LoadTexture(L"../../Assets/Textures/scratched_normals.png", scratchedN);
	LoadTexture(L"../../Assets/Textures/scratched_roughness.png", scratchedR);
	LoadTexture(L"../../Assets/Textures/scratched_metal.png", scratchedM);
	
	LoadTexture(L"../../Assets/Textures/bronze_albedo.png", bronzeA);
	LoadTexture(L"../../Assets/Textures/bronze_normals.png", bronzeN);
	LoadTexture(L"../../Assets/Textures/bronze_roughness.png", bronzeR);
	LoadTexture(L"../../Assets/Textures/bronze_metal.png", bronzeM);
	
	LoadTexture(L"../../Assets/Textures/rough_albedo.png", roughA);
	LoadTexture(L"../../Assets/Textures/rough_normals.png", roughN);
	LoadTexture(L"../../Assets/Textures/rough_roughness.png", roughR);
	LoadTexture(L"../../Assets/Textures/rough_metal.png", roughM);
	
	LoadTexture(L"../../Assets/Textures/wood_albedo.png", woodA);
	LoadTexture(L"../../Assets/Textures/wood_normals.png", woodN);
	LoadTexture(L"../../Assets/Textures/wood_roughness.png", woodR);
	LoadTexture(L"../../Assets/Textures/wood_metal.png", woodM);

	// Describe and create our sampler state
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, samplerOptions.GetAddressOf());

	


	// Create the sky using 6 images
	sky = std::make_shared<Sky>(
		FixPath(L"..\\..\\Assets\\Skies\\Clouds Blue\\right.png").c_str(),
		FixPath(L"..\\..\\Assets\\Skies\\Clouds Blue\\left.png").c_str(),
		FixPath(L"..\\..\\Assets\\Skies\\Clouds Blue\\up.png").c_str(),
		FixPath(L"..\\..\\Assets\\Skies\\Clouds Blue\\down.png").c_str(),
		FixPath(L"..\\..\\Assets\\Skies\\Clouds Blue\\front.png").c_str(),
		FixPath(L"..\\..\\Assets\\Skies\\Clouds Blue\\back.png").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		samplerOptions,
		device,
		context,
		fullscreenVS,
		irrPS, specConvPS, brdfPS);

	// Create non-PBR materials
	std::shared_ptr<Material> cobbleMat2x = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	cobbleMat2x->AddSampler("BasicSampler", samplerOptions);
	cobbleMat2x->AddTextureSRV("Albedo", cobbleA);
	cobbleMat2x->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat2x->AddTextureSRV("RoughnessMap", cobbleR);

	std::shared_ptr<Material> cobbleMat4x = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4x->AddSampler("BasicSampler", samplerOptions);
	cobbleMat4x->AddTextureSRV("Albedo", cobbleA);
	cobbleMat4x->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat4x->AddTextureSRV("RoughnessMap", cobbleR);

	std::shared_ptr<Material> floorMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	floorMat->AddSampler("BasicSampler", samplerOptions);
	floorMat->AddTextureSRV("Albedo", floorA);
	floorMat->AddTextureSRV("NormalMap", floorN);
	floorMat->AddTextureSRV("RoughnessMap", floorR);

	std::shared_ptr<Material> paintMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	paintMat->AddSampler("BasicSampler", samplerOptions);
	paintMat->AddTextureSRV("Albedo", paintA);
	paintMat->AddTextureSRV("NormalMap", paintN);
	paintMat->AddTextureSRV("RoughnessMap", paintR);

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	scratchedMat->AddSampler("BasicSampler", samplerOptions);
	scratchedMat->AddTextureSRV("Albedo", scratchedA);
	scratchedMat->AddTextureSRV("NormalMap", scratchedN);
	scratchedMat->AddTextureSRV("RoughnessMap", scratchedR);

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	bronzeMat->AddSampler("BasicSampler", samplerOptions);
	bronzeMat->AddTextureSRV("Albedo", bronzeA);
	bronzeMat->AddTextureSRV("NormalMap", bronzeN);
	bronzeMat->AddTextureSRV("RoughnessMap", bronzeR);

	std::shared_ptr<Material> roughMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	roughMat->AddSampler("BasicSampler", samplerOptions);
	roughMat->AddTextureSRV("Albedo", roughA);
	roughMat->AddTextureSRV("NormalMap", roughN);
	roughMat->AddTextureSRV("RoughnessMap", roughR);

	std::shared_ptr<Material> woodMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	woodMat->AddSampler("BasicSampler", samplerOptions);
	woodMat->AddTextureSRV("Albedo", woodA);
	woodMat->AddTextureSRV("NormalMap", woodN);
	woodMat->AddTextureSRV("RoughnessMap", woodR);


	// Create PBR materials
	std::shared_ptr<Material> cobbleMat2xPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	cobbleMat2xPBR->AddSampler("BasicSampler", samplerOptions);
	cobbleMat2xPBR->AddSampler("ClampSampler", clampSamplerOptions);
	cobbleMat2xPBR->AddTextureSRV("Albedo", cobbleA);
	cobbleMat2xPBR->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat2xPBR->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat2xPBR->AddTextureSRV("MetalMap", cobbleM);

	std::shared_ptr<Material> cobbleMat4xPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4xPBR->AddSampler("BasicSampler", samplerOptions);
	cobbleMat4xPBR->AddSampler("ClampSampler", clampSamplerOptions);
	cobbleMat4xPBR->AddTextureSRV("Albedo", cobbleA);
	cobbleMat4xPBR->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat4xPBR->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat4xPBR->AddTextureSRV("MetalMap", cobbleM);

	std::shared_ptr<Material> floorMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	floorMatPBR->AddSampler("BasicSampler", samplerOptions);
	floorMatPBR->AddSampler("ClampSampler", clampSamplerOptions);
	floorMatPBR->AddTextureSRV("Albedo", floorA);
	floorMatPBR->AddTextureSRV("NormalMap", floorN);
	floorMatPBR->AddTextureSRV("RoughnessMap", floorR);
	floorMatPBR->AddTextureSRV("MetalMap", floorM);

	std::shared_ptr<Material> paintMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	paintMatPBR->AddSampler("BasicSampler", samplerOptions);
	paintMatPBR->AddSampler("ClampSampler", clampSamplerOptions);
	paintMatPBR->AddTextureSRV("Albedo", paintA);
	paintMatPBR->AddTextureSRV("NormalMap", paintN);
	paintMatPBR->AddTextureSRV("RoughnessMap", paintR);
	paintMatPBR->AddTextureSRV("MetalMap", paintM);

	std::shared_ptr<Material> scratchedMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	scratchedMatPBR->AddSampler("BasicSampler", samplerOptions);
	scratchedMatPBR->AddSampler("ClampSampler", clampSamplerOptions);
	scratchedMatPBR->AddTextureSRV("Albedo", scratchedA);
	scratchedMatPBR->AddTextureSRV("NormalMap", scratchedN);
	scratchedMatPBR->AddTextureSRV("RoughnessMap", scratchedR);
	scratchedMatPBR->AddTextureSRV("MetalMap", scratchedM);

	std::shared_ptr<Material> bronzeMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	bronzeMatPBR->AddSampler("BasicSampler", samplerOptions);
	bronzeMatPBR->AddSampler("ClampSampler", clampSamplerOptions);
	bronzeMatPBR->AddTextureSRV("Albedo", bronzeA);
	bronzeMatPBR->AddTextureSRV("NormalMap", bronzeN);
	bronzeMatPBR->AddTextureSRV("RoughnessMap", bronzeR);
	bronzeMatPBR->AddTextureSRV("MetalMap", bronzeM);

	std::shared_ptr<Material> roughMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	roughMatPBR->AddSampler("BasicSampler", samplerOptions);
	roughMatPBR->AddSampler("ClampSampler", clampSamplerOptions);
	roughMatPBR->AddTextureSRV("Albedo", roughA);
	roughMatPBR->AddTextureSRV("NormalMap", roughN);
	roughMatPBR->AddTextureSRV("RoughnessMap", roughR);
	roughMatPBR->AddTextureSRV("MetalMap", roughM);

	std::shared_ptr<Material> woodMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	woodMatPBR->AddSampler("BasicSampler", samplerOptions);
	woodMatPBR->AddSampler("ClampSampler", clampSamplerOptions);
	woodMatPBR->AddTextureSRV("Albedo", woodA);
	woodMatPBR->AddTextureSRV("NormalMap", woodN);
	woodMatPBR->AddTextureSRV("RoughnessMap", woodR);
	woodMatPBR->AddTextureSRV("MetalMap", woodM);



	// === Create the PBR entities =====================================
	std::shared_ptr<GameEntity> cobSpherePBR = std::make_shared<GameEntity>(sphereMesh, cobbleMat2xPBR);
	cobSpherePBR->GetTransform()->SetPosition(-3, 3.75, 0);
	cobSpherePBR->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> floorSpherePBR = std::make_shared<GameEntity>(sphereMesh, floorMatPBR);
	floorSpherePBR->GetTransform()->SetPosition(-4, 2, 0);
	floorSpherePBR->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> paintSpherePBR = std::make_shared<GameEntity>(sphereMesh, paintMatPBR);
	paintSpherePBR->GetTransform()->SetPosition(-2, 2, 0);
	paintSpherePBR->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> scratchSpherePBR = std::make_shared<GameEntity>(sphereMesh, scratchedMatPBR);
	scratchSpherePBR->GetTransform()->SetPosition(0, 2, 0);
	scratchSpherePBR->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> bronzeSpherePBR = std::make_shared<GameEntity>(sphereMesh, bronzeMatPBR);
	bronzeSpherePBR->GetTransform()->SetPosition(2, 2, 0);
	bronzeSpherePBR->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> roughSpherePBR = std::make_shared<GameEntity>(sphereMesh, roughMatPBR);
	roughSpherePBR->GetTransform()->SetPosition(-1, 3.75, 0);
	roughSpherePBR->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> woodSpherePBR = std::make_shared<GameEntity>(sphereMesh, woodMatPBR);
	woodSpherePBR->GetTransform()->SetPosition(1, 3.75, 0);
	woodSpherePBR->GetTransform()->SetScale(2, 2, 2);

	entities.push_back(cobSpherePBR);
	entities.push_back(floorSpherePBR);
	entities.push_back(paintSpherePBR);
	entities.push_back(scratchSpherePBR);
	entities.push_back(bronzeSpherePBR);
	entities.push_back(roughSpherePBR);
	entities.push_back(woodSpherePBR);

	// Create the non-PBR entities ==============================
	std::shared_ptr<GameEntity> cobSphere = std::make_shared<GameEntity>(sphereMesh, cobbleMat2x);
	cobSphere->GetTransform()->SetPosition(-4, 3.75, 1.75);
	cobSphere->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> floorSphere = std::make_shared<GameEntity>(sphereMesh, floorMat);
	floorSphere->GetTransform()->SetPosition(-5, 2, 1.75);
	floorSphere->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> paintSphere = std::make_shared<GameEntity>(sphereMesh, paintMat);
	paintSphere->GetTransform()->SetPosition(-3, 2, 1.75);
	paintSphere->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> scratchSphere = std::make_shared<GameEntity>(sphereMesh, scratchedMat);
	scratchSphere->GetTransform()->SetPosition(-1, 2, 1.75);
	scratchSphere->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> bronzeSphere = std::make_shared<GameEntity>(sphereMesh, bronzeMat);
	bronzeSphere->GetTransform()->SetPosition(1, 2, 1.75);
	bronzeSphere->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> roughSphere = std::make_shared<GameEntity>(sphereMesh, roughMat);
	roughSphere->GetTransform()->SetPosition(-2, 3.75, 1.75);
	roughSphere->GetTransform()->SetScale(2, 2, 2);

	std::shared_ptr<GameEntity> woodSphere = std::make_shared<GameEntity>(sphereMesh, woodMat);
	woodSphere->GetTransform()->SetPosition(0, 3.75, 1.75);
	woodSphere->GetTransform()->SetScale(2, 2, 2);

	entities.push_back(cobSphere);
	entities.push_back(floorSphere);
	entities.push_back(paintSphere);
	entities.push_back(scratchSphere);
	entities.push_back(bronzeSphere);
	entities.push_back(roughSphere);
	entities.push_back(woodSphere);


	// Save assets needed for drawing point lights
	lightMesh = sphereMesh;
	lightVS = vertexShader;
	lightPS = solidColorPS;


	




}


// --------------------------------------------------------
// Generates the lights in the scene: 3 directional lights
// and many random point lights.
// --------------------------------------------------------
void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dir1.Intensity = .4f;

	Light dir2 = {};
	dir2.Type = LIGHT_TYPE_DIRECTIONAL;
	dir2.Direction = XMFLOAT3(-1, -0.25f, 0);
	dir2.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir2.Intensity = 0.2f;

	Light dir3 = {};
	dir3.Type = LIGHT_TYPE_DIRECTIONAL;
	dir3.Direction = XMFLOAT3(0, -1, 1);
	dir3.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir3.Intensity = 0.1f;

	// Add light to the list
	lights.push_back(dir1);
	lights.push_back(dir2);
	lights.push_back(dir3);

	// Create the rest of the lights
	while (lights.size() < MAX_LIGHTS)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-10.0f, 10.0f), RandomRange(-5.0f, 5.0f), RandomRange(-10.0f, 10.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.1f, 0.5f);

		// Add to the list
		lights.push_back(point);
	}

}



// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Need to resize the render targets
	sceneNormalsRTV.Reset();
	sceneNormalsSRV.Reset();
	sceneColorsRTV.Reset();
	sceneColorsSRV.Reset();
	sceneAmbientRTV.Reset();
	sceneAmbientSRV.Reset();

	ssaoRTV.Reset();
	ssaoSRV.Reset();
	blurRTV.Reset();
	blurSRV.Reset();

	depthRTV.Reset();
	depthSRV.Reset();

	Microsoft::WRL::ComPtr<ID3D11Texture2D> rtTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneColorsTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ambientTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ssaoTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ssaoBlurTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = windowWidth;
	texDesc.Height = windowHeight;
	texDesc.ArraySize = 1;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Need both!
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Might occasionally use other formats
	texDesc.MipLevels = 1; // Usually no mip chain needed for render targets
	texDesc.MiscFlags = 0;
	texDesc.SampleDesc.Count = 1; // Can't be zero
	device->CreateTexture2D(&texDesc, 0, rtTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture
	device->CreateTexture2D(&texDesc, 0, sceneColorsTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture
	device->CreateTexture2D(&texDesc, 0, ambientTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture
	device->CreateTexture2D(&texDesc, 0, ssaoTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture
	device->CreateTexture2D(&texDesc, 0, ssaoBlurTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture
	//device->CreateTexture2D(&texDesc, 0, depthTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture

	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = windowWidth;
	depthDesc.Height = windowHeight;
	depthDesc.ArraySize = 1;
	depthDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Need both!
	depthDesc.Format = DXGI_FORMAT_R32_FLOAT; // Might occasionally use other formats
	depthDesc.MipLevels = 1; // Usually no mip chain needed for render targets
	depthDesc.MiscFlags = 0;
	depthDesc.SampleDesc.Count = 1; // Can't be zero
	depthDesc.SampleDesc.Quality = 0;
	device->CreateTexture2D(&depthDesc, 0, depthTexture.GetAddressOf()); // ComPtr<ID3D11Texture2D> rtTexture

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // This points to a Texture2D
	rtvDesc.Texture2D.MipSlice = 0; // Which mip are we rendering into?
	rtvDesc.Format = texDesc.Format; // Same format as texture
	device->CreateRenderTargetView(rtTexture.Get(), &rtvDesc, sceneNormalsRTV.GetAddressOf());
	device->CreateRenderTargetView(sceneColorsTexture.Get(), &rtvDesc, sceneColorsRTV.GetAddressOf());

	D3D11_RENDER_TARGET_VIEW_DESC rtvDepthDesc = {};
	rtvDepthDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // This points to a Texture2D
	rtvDepthDesc.Texture2D.MipSlice = 0; // Which mip are we rendering into?
	rtvDepthDesc.Format = depthDesc.Format; // Same format as texture

	device->CreateRenderTargetView(depthTexture.Get(), &rtvDepthDesc, depthRTV.GetAddressOf());
	device->CreateShaderResourceView(depthTexture.Get(), 0, depthSRV.GetAddressOf());

	device->CreateShaderResourceView(
		rtTexture.Get(), // Texture resource itself
		0, // Null description = default SRV options
		sceneNormalsSRV.GetAddressOf()); // ComPtr<ID3D11ShaderResourceView>

	device->CreateShaderResourceView(
		sceneColorsTexture.Get(), // Texture resource itself
		0, // Null description = default SRV options
		sceneColorsSRV.GetAddressOf()); // ComPtr<ID3D11ShaderResourceView>


	device->CreateRenderTargetView(ambientTexture.Get(), &rtvDesc, sceneAmbientRTV.GetAddressOf());
	device->CreateShaderResourceView(ambientTexture.Get(), 0, sceneAmbientSRV.GetAddressOf());

	device->CreateRenderTargetView(ssaoTexture.Get(), &rtvDesc, ssaoRTV.GetAddressOf());
	device->CreateShaderResourceView(ssaoTexture.Get(), 0, ssaoSRV.GetAddressOf());

	device->CreateRenderTargetView(ssaoBlurTexture.Get(), &rtvDesc, blurRTV.GetAddressOf());
	device->CreateShaderResourceView(ssaoBlurTexture.Get(), 0, blurSRV.GetAddressOf());



	


	// Update our projection matrix to match the new aspect ratio
	if (camera)
		camera->UpdateProjectionMatrix(this->windowWidth / (float)this->windowHeight);
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Set up the new frame for the UI, then build
	// this frame's interface.  Note that the building
	// of the UI could happen at any point during update.
	UINewFrame(deltaTime);
	BuildUI();

	// Update the camera
	camera->Update(deltaTime);

	// Check individual input
	Input& input = Input::GetInstance();
	if (input.KeyDown(VK_ESCAPE)) Quit();
	if (input.KeyPress(VK_TAB)) GenerateLights();
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Frame START
	// - These things should happen ONCE PER FRAME
	// - At the beginning of Game::Draw() before drawing *anything*
	{
		// Clear the back buffer (erases what's on the screen)
		const float bgColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Black
		context->ClearRenderTargetView(backBufferRTV.Get(), bgColor);
		context->ClearRenderTargetView(sceneColorsRTV.Get(), bgColor);
		context->ClearRenderTargetView(sceneNormalsRTV.Get(), bgColor);
		context->ClearRenderTargetView(sceneAmbientRTV.Get(), bgColor);
		context->ClearRenderTargetView(depthRTV.Get(), bgColor);
		// Clear the depth buffer (resets per-pixel occlusion information)
		context->ClearDepthStencilView(depthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

		ID3D11RenderTargetView* renderTargets[4] = {};
		renderTargets[0] = sceneColorsRTV.Get();
		renderTargets[1] = sceneNormalsRTV.Get();
		renderTargets[2] = sceneAmbientRTV.Get();
		renderTargets[3] = depthRTV.Get();
		context->OMSetRenderTargets(4, renderTargets, depthBufferDSV.Get());
	}


	// Draw all of the entities
	for (auto& ge : entities)
	{
		// Set the "per frame" data
		// Note that this should literally be set once PER FRAME, before
		// the draw loop, but we're currently setting it per entity since 
		// we are just using whichever shader the current entity has.  
		// Inefficient!!!
		std::shared_ptr<SimplePixelShader> ps = ge->GetMaterial()->GetPixelShader();
		ps->SetData("lights", (void*)(&lights[0]), sizeof(Light) * lightCount);
		ps->SetInt("lightCount", lightCount);
		ps->SetFloat3("cameraPosition", camera->GetTransform()->GetPosition());
		ps->CopyBufferData("perFrame");
		ps->SetInt("specIBLTotalMipLevels", sky->GetTotalSpecularIBLMipLevels());
		ps->SetShaderResourceView("IrradianceIBLMap", sky->GetIrradianceMap());
		ps->SetShaderResourceView("SpecularIBLMap", sky->GetSpecularMap());
		ps->SetShaderResourceView("BrdfLookUpMap", sky->GetBRDFLookUpTexture());
		ps->SetSamplerState("BasicSampler", samplerOptions);
		ps->SetSamplerState("ClampSampler", clampSamplerOptions);
		

		// Draw the entity
		ge->Draw(context, camera);
	}

	// Draw the light sources?
	if(showPointLights)
		DrawPointLights();

	// Draw the sky
	sky->Draw(camera);

	
	/*context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), 0);

	fullscreenVS->SetShader();
	simpleTexturePS->SetShader();
	simpleTexturePS->SetShaderResourceView("Pixels", sceneColorsSRV);

	context->Draw(3, 0);*/




	// SSAO POST PROCESSING
	{
		const int numTargets = 4;
		ID3D11RenderTargetView* renderTargets[4] = {};
		context->OMSetRenderTargets(4, renderTargets, 0);

		fullscreenVS->SetShader();

		context->OMSetRenderTargets(1, ssaoRTV.GetAddressOf(), 0);

		// SSAO RESULTS
		ssaoPS->SetShader();
		
		XMFLOAT4X4 invView, invProj, view = camera->GetView(), proj = camera->GetProjection();
		XMStoreFloat4x4(&invView, XMMatrixInverse(0, XMLoadFloat4x4(&view)));
		XMStoreFloat4x4(&invProj, XMMatrixInverse(0, XMLoadFloat4x4(&proj)));
		ssaoPS->SetMatrix4x4("invViewMatrix", invView);
		ssaoPS->SetMatrix4x4("invProjMatrix", invProj);
		ssaoPS->SetMatrix4x4("viewMatrix", view);
		ssaoPS->SetMatrix4x4("projectionMatrix", proj);
		ssaoPS->SetData("offsets", ssaoOffsets, sizeof(XMFLOAT4) * ARRAYSIZE(ssaoOffsets));
		ssaoPS->SetFloat("ssaoRadius", ssaoRadius);
		ssaoPS->SetInt("ssaoSamples", ssaoSamples);
		ssaoPS->SetFloat2("randomTextureScreenScale", XMFLOAT2(windowWidth / 4.0f, windowHeight / 4.0f));
		ssaoPS->CopyAllBufferData();

		ssaoPS->SetShaderResourceView("Normals", sceneNormalsSRV);
		ssaoPS->SetShaderResourceView("Depths", depthSRV);
		ssaoPS->SetShaderResourceView("Random", randomTexSRV);
		ssaoPS->SetSamplerState("BasicSampler", samplerOptions);
		ssaoPS->SetSamplerState("ClampSampler", clampSamplerOptions);

		context->Draw(3, 0);


		renderTargets[0] = blurRTV.Get();
		context->OMSetRenderTargets(1, renderTargets, 0);
		blurPS->SetShader();
		blurPS->SetShaderResourceView("SSAO", ssaoSRV);
		blurPS->SetSamplerState("ClampSampler", clampSamplerOptions);
		blurPS->SetFloat2("pixelSize", XMFLOAT2(1.0f / windowWidth, 1.0f / windowHeight));
		blurPS->CopyAllBufferData();
		context->Draw(3, 0);

		renderTargets[0] = backBufferRTV.Get();
		context->OMSetRenderTargets(1, renderTargets, 0);

		combinePS->SetShader();
		combinePS->SetShaderResourceView("SceneColorsNoAmbient", sceneColorsSRV);
		combinePS->SetShaderResourceView("Ambient", sceneAmbientSRV);
		combinePS->SetShaderResourceView("SSAOBlur", blurSRV);
		combinePS->SetSamplerState("BasicSampler", samplerOptions);
		combinePS->SetFloat2("pixelSize", XMFLOAT2(1.0f / windowWidth, 1.0f / windowHeight));
		combinePS->CopyAllBufferData();
		context->Draw(3, 0);



		context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), 0);

		
	}

	ID3D11ShaderResourceView* nullSRVs[128] = {};
	context->PSSetShaderResources(0, 128, nullSRVs);
	


	// Frame END
	// - These should happen exactly ONCE PER FRAME
	// - At the very end of the frame (after drawing *everything*)
	{
		// Draw the UI after everything else
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		// Present the back buffer to the user
		//  - Puts the results of what we've drawn onto the window
		//  - Without this, the user never sees anything
		bool vsyncNecessary = vsync || !deviceSupportsTearing || isFullscreen;
		swapChain->Present(
			vsyncNecessary ? 1 : 0,
			vsyncNecessary ? 0 : DXGI_PRESENT_ALLOW_TEARING);

		// Must re-bind buffers after presenting, as they become unbound
		context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());

		

	}
}


// --------------------------------------------------------
// Draws the point lights as solid color spheres
// --------------------------------------------------------
void Game::DrawPointLights()
{
	// Turn on these shaders
	lightVS->SetShader();
	lightPS->SetShader();

	// Set up vertex shader
	lightVS->SetMatrix4x4("view", camera->GetView());
	lightVS->SetMatrix4x4("projection", camera->GetProjection());

	for (int i = 0; i < lightCount; i++)
	{
		Light light = lights[i];

		// Only drawing points, so skip others
		if (light.Type != LIGHT_TYPE_POINT)
			continue;

		// Calc quick scale based on range
		float scale = light.Range / 20.0f;

		// Make the transform for this light
		XMMATRIX rotMat = XMMatrixIdentity();
		XMMATRIX scaleMat = XMMatrixScaling(scale, scale, scale);
		XMMATRIX transMat = XMMatrixTranslation(light.Position.x, light.Position.y, light.Position.z);
		XMMATRIX worldMat = scaleMat * rotMat * transMat;

		XMFLOAT4X4 world;
		XMFLOAT4X4 worldInvTrans;
		DirectX::XMStoreFloat4x4(&world, worldMat);
		DirectX::XMStoreFloat4x4(&worldInvTrans, XMMatrixInverse(0, XMMatrixTranspose(worldMat)));

		// Set up the world matrix for this light
		lightVS->SetMatrix4x4("world", world);
		lightVS->SetMatrix4x4("worldInverseTranspose", worldInvTrans);

		// Set up the pixel shader data
		XMFLOAT3 finalColor = light.Color;
		finalColor.x *= light.Intensity;
		finalColor.y *= light.Intensity;
		finalColor.z *= light.Intensity;
		lightPS->SetFloat3("Color", finalColor);

		// Copy data
		lightVS->CopyAllBufferData();
		lightPS->CopyAllBufferData();

		// Draw
		lightMesh->SetBuffersAndDraw(context);
	}

}



// --------------------------------------------------------
// Prepares a new frame for the UI, feeding it fresh
// input and time information for this new frame.
// --------------------------------------------------------
void Game::UINewFrame(float deltaTime)
{
	// Get a reference to our custom input manager
	Input& input = Input::GetInstance();

	// Reset input manager's gui state so we don’t
	// taint our own input
	input.SetKeyboardCapture(false);
	input.SetMouseCapture(false);

	// Feed fresh input data to ImGui
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = deltaTime;
	io.DisplaySize.x = (float)this->windowWidth;
	io.DisplaySize.y = (float)this->windowHeight;

	// Reset the frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Determine new input capture
	input.SetKeyboardCapture(io.WantCaptureKeyboard);
	input.SetMouseCapture(io.WantCaptureMouse);
}


// --------------------------------------------------------
// Builds the UI for the current frame
// --------------------------------------------------------
void Game::BuildUI()
{
	// Should we show the built-in demo window?
	if (showUIDemoWindow)
	{
		ImGui::ShowDemoWindow();
	}

	// Actually build our custom UI, starting with a window
	ImGui::Begin("Inspector");
	{
		// Set a specific amount of space for widget labels
		ImGui::PushItemWidth(-160); // Negative value sets label width

		// === Overall details ===
		if (ImGui::TreeNode("App Details"))
		{
			ImGui::Spacing();
			ImGui::Text("Frame rate: %f fps", ImGui::GetIO().Framerate);
			ImGui::Text("Window Client Size: %dx%d", windowWidth, windowHeight);

			ImGui::Spacing();
			ImGui::Text("Scene Details");
			ImGui::Text("Top Row:");    ImGui::SameLine(125); ImGui::Text("PBR Materials");
			ImGui::Text("Bottom Row:"); ImGui::SameLine(125); ImGui::Text("Non-PBR Materials");
	
			// Should we show the demo window?
			ImGui::Spacing();
			if (ImGui::Button(showUIDemoWindow ? "Hide ImGui Demo Window" : "Show ImGui Demo Window"))
				showUIDemoWindow = !showUIDemoWindow;

			ImGui::Spacing();

			// Finalize the tree node
			ImGui::TreePop();
		}
		
		// === Controls ===
		if (ImGui::TreeNode("Controls"))
		{
			ImGui::Spacing();
			ImGui::Text("(WASD, X, Space)");    ImGui::SameLine(175); ImGui::Text("Move camera");
			ImGui::Text("(Left Click & Drag)"); ImGui::SameLine(175); ImGui::Text("Rotate camera");
			ImGui::Text("(Left Shift)");        ImGui::SameLine(175); ImGui::Text("Hold to speed up camera");
			ImGui::Text("(Left Ctrl)");         ImGui::SameLine(175); ImGui::Text("Hold to slow down camera");
			ImGui::Text("(TAB)");               ImGui::SameLine(175); ImGui::Text("Randomize lights");
			ImGui::Spacing();

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Camera details ===
		if (ImGui::TreeNode("Camera"))
		{
			// Show UI for current camera
			CameraUI(camera);

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Entities ===
		if (ImGui::TreeNode("Scene Entities"))
		{
			// Loop and show the details for each entity
			for (int i = 0; i < entities.size(); i++)
			{
				// New node for each entity
				// Note the use of PushID(), so that each tree node and its widgets
				// have unique internal IDs in the ImGui system
				ImGui::PushID(i);
				if (ImGui::TreeNode("Entity Node", "Entity %d", i))
				{
					// Build UI for one entity at a time
					EntityUI(entities[i]);

					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Lights ===
		if (ImGui::TreeNode("Lights"))
		{
			// Light details
			ImGui::Spacing();
			ImGui::SliderInt("Light Count", &lightCount, 0, MAX_LIGHTS);
			ImGui::Checkbox("Show Point Lights", &showPointLights);
			ImGui::Spacing();

			// Loop and show the details for each entity
			for (int i = 0; i < lightCount; i++)
			{
				// Name of this light based on type
				std::string lightName = "Light %d";
				switch (lights[i].Type)
				{
				case LIGHT_TYPE_DIRECTIONAL: lightName += " (Directional)"; break;
				case LIGHT_TYPE_POINT: lightName += " (Point)"; break;
				case LIGHT_TYPE_SPOT: lightName += " (Spot)"; break;
				}

				// New node for each light
				// Note the use of PushID(), so that each tree node and its widgets
				// have unique internal IDs in the ImGui system
				ImGui::PushID(i);
				if (ImGui::TreeNode("Light Node", lightName.c_str(), i))
				{
					// Build UI for one entity at a time
					LightUI(lights[i]);

					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === RenderTargets ===
		if (ImGui::TreeNode("Render Targets"))
		{
			ImVec2 size = ImGui::GetItemRectSize();
			float rtHeight = size.x * ((float)windowHeight / windowWidth);

			ImGui::Image(sceneColorsSRV.Get(), ImVec2(size.x * 2, rtHeight * 2));
			ImGui::Image(sceneNormalsSRV.Get(), ImVec2(size.x * 2, rtHeight * 2));
			ImGui::Image(sceneAmbientSRV.Get(), ImVec2(size.x * 2, rtHeight * 2));
			ImGui::Image(depthSRV.Get(), ImVec2(size.x * 2, rtHeight * 2));
			ImGui::Image(ssaoSRV.Get(), ImVec2(size.x * 2, rtHeight * 2));
			ImGui::Image(blurSRV.Get(), ImVec2(size.x * 2, rtHeight * 2));

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("SSAO Settings"))
		{
			ImGui::SliderInt("SSAO Samples", &ssaoSamples, 1, 64);
			ImGui::SliderFloat("Radius", &ssaoRadius, 0.0f, 5.0f);



			ImGui::TreePop();
		}
	}
	ImGui::End();
}


// --------------------------------------------------------
// Builds the UI for a single camera
// --------------------------------------------------------
void Game::CameraUI(std::shared_ptr<Camera> cam)
{
	ImGui::Spacing();

	// Transform details
	XMFLOAT3 pos = cam->GetTransform()->GetPosition();
	XMFLOAT3 rot = cam->GetTransform()->GetPitchYawRoll();

	if (ImGui::DragFloat3("Position", &pos.x, 0.01f))
		cam->GetTransform()->SetPosition(pos);
	if (ImGui::DragFloat3("Rotation (Radians)", &rot.x, 0.01f))
		cam->GetTransform()->SetRotation(rot);
	ImGui::Spacing();

	// Clip planes
	float nearClip = cam->GetNearClip();
	float farClip = cam->GetFarClip();
	if (ImGui::DragFloat("Near Clip Distance", &nearClip, 0.01f, 0.001f, 1.0f))
		cam->SetNearClip(nearClip);
	if (ImGui::DragFloat("Far Clip Distance", &farClip, 1.0f, 10.0f, 1000.0f))
		cam->SetFarClip(farClip);

	// Projection type
	CameraProjectionType projType = cam->GetProjectionType();
	int typeIndex = (int)projType;
	if (ImGui::Combo("Projection Type", &typeIndex, "Perspective\0Orthographic"))
	{
		projType = (CameraProjectionType)typeIndex;
		cam->SetProjectionType(projType);
	}

	// Projection details
	if (projType == CameraProjectionType::Perspective)
	{
		// Convert field of view to degrees for UI
		float fov = cam->GetFieldOfView() * 180.0f / XM_PI;
		if (ImGui::SliderFloat("Field of View (Degrees)", &fov, 0.01f, 180.0f))
			cam->SetFieldOfView(fov * XM_PI / 180.0f); // Back to radians
	}
	else if (projType == CameraProjectionType::Orthographic)
	{
		float wid = cam->GetOrthographicWidth();
		if (ImGui::SliderFloat("Orthographic Width", &wid, 1.0f, 10.0f))
			cam->SetOrthographicWidth(wid);
	}

	ImGui::Spacing();
}


// --------------------------------------------------------
// Builds the UI for a single entity
// --------------------------------------------------------
void Game::EntityUI(std::shared_ptr<GameEntity> entity)
{
	ImGui::Spacing();

	// Transform details
	Transform* trans = entity->GetTransform();
	XMFLOAT3 pos = trans->GetPosition();
	XMFLOAT3 rot = trans->GetPitchYawRoll();
	XMFLOAT3 sca = trans->GetScale();

	if (ImGui::DragFloat3("Position", &pos.x, 0.01f)) trans->SetPosition(pos);
	if (ImGui::DragFloat3("Rotation (Radians)", &rot.x, 0.01f)) trans->SetRotation(rot);
	if (ImGui::DragFloat3("Scale", &sca.x, 0.01f)) trans->SetScale(sca);

	// Mesh details
	ImGui::Spacing();
	ImGui::Text("Mesh Index Count: %d", entity->GetMesh()->GetIndexCount());

	ImGui::Spacing();
}


// --------------------------------------------------------
// Builds the UI for a single light
// --------------------------------------------------------
void Game::LightUI(Light& light)
{
	// Light type
	if (ImGui::RadioButton("Directional", light.Type == LIGHT_TYPE_DIRECTIONAL))
	{
		light.Type = LIGHT_TYPE_DIRECTIONAL;
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Point", light.Type == LIGHT_TYPE_POINT))
	{
		light.Type = LIGHT_TYPE_POINT;
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Spot", light.Type == LIGHT_TYPE_SPOT))
	{
		light.Type = LIGHT_TYPE_SPOT;
	}

	// Direction
	if (light.Type == LIGHT_TYPE_DIRECTIONAL || light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::DragFloat3("Direction", &light.Direction.x, 0.1f);

		// Normalize the direction
		XMVECTOR dirNorm = XMVector3Normalize(XMLoadFloat3(&light.Direction));
		XMStoreFloat3(&light.Direction, dirNorm);
	}

	// Position & Range
	if (light.Type == LIGHT_TYPE_POINT || light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::DragFloat3("Position", &light.Position.x, 0.1f);
		ImGui::SliderFloat("Range", &light.Range, 0.1f, 100.0f);
	}

	// Spot falloff
	if (light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::SliderFloat("Spot Falloff", &light.SpotFalloff, 0.1f, 128.0f);
	}

	// Color details
	ImGui::ColorEdit3("Color", &light.Color.x);
	ImGui::SliderFloat("Intensity", &light.Intensity, 0.0f, 10.0f);
}


