#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include "Material.h"
#include "Camera.h"
struct Particle
{
	float age;
	float startPos;
};




class Emitter
{
public:
	Emitter(int maxParticles, int particlesPerSecond, float lifetime, float startSize, float endSize, DirectX::XMFLOAT4 startColor, DirectX::XMFLOAT4 endColor);
	~Emitter();
	void Update(float dt);
	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera);
private:
	Particle* particles;
	int firstAliveParticle;
	int firstDeadParticle;
	int numParticles;
	int maxParticles;

	float lifetime;
	int particlesPerSecond;
	float secondsPerParticle;
	float lastParticleTime;

	// Addition Stuff
	float startSize;
	float endSize;
	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;

	//Buffers
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	DirectX::XMFLOAT2 UVs[4];


	std::shared_ptr<Material> mat;
};

