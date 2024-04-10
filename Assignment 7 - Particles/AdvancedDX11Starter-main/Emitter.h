#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include "Material.h"
#include "Camera.h"
struct Particle
{
	float emitTime;
	DirectX::XMFLOAT3 startPos;
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 startVel;
	float pad;
};


class Emitter
{
public:
	Emitter(int maxParticles, int particlesPerSecond, float lifetime, Microsoft::WRL::ComPtr<ID3D11Device> device, std::shared_ptr<Material> material, DirectX::XMFLOAT3 startVel = DirectX::XMFLOAT3(0, 1, 0), DirectX::XMFLOAT3 acceleration = DirectX::XMFLOAT3(0, 0, 0));
	~Emitter();
	void Update(float dt, float currentTime);
	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera, float currentTime);

	DirectX::XMFLOAT3 startVel;
	DirectX::XMFLOAT3 acceleration;
private:
	int maxParticles;
	Particle* particles;

	int firstDeadParticle;
	int firstLivingParticle;
	int numLivingParticles;

	float lifetime;
	int particlesPerSecond;
	float secondsPerParticle;
	float lastEmitTime;

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleSRV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	void UpdateParticle(int index, float currentTime);
	void SpawnParticle(float currentTime);

	Transform transform;
	std::shared_ptr<Material> mat;
};

