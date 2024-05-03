#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "SimpleShader.h"
#include "Lights.h"
#include "Sky.h"

#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>

class Game 
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	void Init();
	void OnResize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);

private:

	// Our scene
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::shared_ptr<Camera> camera;

	// Lights
	std::vector<Light> lights;
	int lightCount;
	bool showPointLights;

	// These will be loaded along with other assets and
	// saved to these variables for ease of access
	std::shared_ptr<Mesh> lightMesh;
	std::shared_ptr<SimpleVertexShader> lightVS;
	std::shared_ptr<SimplePixelShader> lightPS;

	// Texture related resources
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> clampSamplerOptions;

	
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneNormalsRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneNormalsSRV;

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneColorsRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneColorsSRV;

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneAmbientRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneAmbientSRV;

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> ssaoRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ssaoSRV;

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> blurRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blurSRV;

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> depthRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthSRV;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> randomTexSRV;
	

	std::shared_ptr<SimpleVertexShader> fullscreenVS;
	std::shared_ptr<SimplePixelShader> solidColorPS;
	std::shared_ptr<SimplePixelShader> simpleTexturePS;

	std::shared_ptr<SimplePixelShader> ssaoPS;
	std::shared_ptr<SimplePixelShader> blurPS;
	std::shared_ptr<SimplePixelShader> combinePS;

	DirectX::XMFLOAT4 ssaoOffsets[64];

	// Skybox
	std::shared_ptr<Sky> sky;

	// General helpers for setup and drawing
	void LoadAssetsAndCreateEntities();
	void GenerateLights();
	void DrawPointLights();

	// UI functions
	void UINewFrame(float deltaTime);
	void BuildUI();
	void CameraUI(std::shared_ptr<Camera> cam);
	void EntityUI(std::shared_ptr<GameEntity> entity);	
	void LightUI(Light& light);
	
	// Should the ImGui demo window be shown?
	bool showUIDemoWindow;

	int ssaoSamples;
	float ssaoRadius;
};

