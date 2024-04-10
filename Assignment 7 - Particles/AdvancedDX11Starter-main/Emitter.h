#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include "Material.h"
#include "Camera.h"
#include "Transform.h"
#include "SimpleShader.h"
struct Particle
{
	float emitTime;
	DirectX::XMFLOAT3 startPos;
	float pad;
	DirectX::XMFLOAT3 startVel;
};


class Emitter
{
public:
	Emitter(Microsoft::WRL::ComPtr<ID3D11Device> device, std::shared_ptr<Material> material, int maxParticles, int particlesPerSecond, float lifetime, DirectX::XMFLOAT3 startVelocity = DirectX::XMFLOAT3(0, 1, 0));
	~Emitter();
	void Update(float dt, float currentTime);
	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera, float currentTime);

	float lifetime;

	DirectX::XMFLOAT3 startVelocity;


	Transform* GetTransform();
	std::shared_ptr<Material> GetMaterial();
	void SetMaterial(std::shared_ptr<Material> material);


private:
	int particlesPerSecond;
	float secondsPerParticle;
	float lastEmitTime;

	Particle* particles;
	int maxParticles;

	int firstAliveIndex;
	int firstDeadIndex;
	int numLiving;

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleDataSRV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	Transform transform;
	std::shared_ptr<Material> material;

	void UpdateParticle(float currentTime, int i);
	void SpawnParticle(float currentTime);
};

