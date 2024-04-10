#pragma once

#include <wrl/client.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>
#include "Camera.h"
#include "Material.h"
#include "Transform.h"


struct Particle
{
	float EmitTime;
	DirectX::XMFLOAT3 StartPosition;
	DirectX::XMFLOAT3 StartVelocity;
};

class Emitter
{
public:
	Emitter(
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		std::shared_ptr<Material> material,
		int maxParticles,
		int particlesPerSecond,
		float lifetime,
		float startSize = 1.0f,
		float endSize = 1.0f,
		DirectX::XMFLOAT4 startColor = DirectX::XMFLOAT4(1, 1, 1, 1),
		DirectX::XMFLOAT4 endColor = DirectX::XMFLOAT4(1, 1, 1, 1),
		bool isbox = false,
		DirectX::XMFLOAT3 emitterPosition = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 startVelocity = DirectX::XMFLOAT3(0, 1, 0),
		DirectX::XMFLOAT3 acceleration = DirectX::XMFLOAT3(0, 0, 0)
	);
	~Emitter();

	void Update(float dt, float currentTime);
	void Draw(
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
		std::shared_ptr<Camera> camera,
		float currentTime);

	float lifetime;

	DirectX::XMFLOAT3 acceleration;
	DirectX::XMFLOAT3 startVelocity;

	
	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
	float startSize;
	float endSize;
	bool isBox;
	

	Transform* GetTransform();
	std::shared_ptr<Material> GetMaterial();
private:
	
	int particlesPerSecond;
	float secondsPerParticle;
	float timeSinceLastEmit;

	Particle* particles;
	int maxParticles;

	int firstDeadIndex;
	int firstAliveIndex;
	int numLiving;

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleDataSRV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	Transform transform;
	std::shared_ptr<Material> material;

	void UpdateParticle(float currentTime, int index);
	void SpawnParticle(float currentTime);
};

