#include "Emitter.h"

Emitter::Emitter(Microsoft::WRL::ComPtr<ID3D11Device> device, std::shared_ptr<Material> material, int maxParticles, int particlesPerSecond, float lifetime, DirectX::XMFLOAT3 startVelocity) :
	device(device), material(material), maxParticles(maxParticles), particlesPerSecond(particlesPerSecond), lifetime(lifetime), startVelocity(startVelocity), particles(0)
{
}

Emitter::~Emitter()
{
}

void Emitter::Update(float dt, float currentTime)
{
}

void Emitter::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera, float currentTime)
{
}

Transform* Emitter::GetTransform()
{
	return nullptr;
}

std::shared_ptr<Material> Emitter::GetMaterial()
{
	return std::shared_ptr<Material>();
}

void Emitter::SetMaterial(std::shared_ptr<Material> material)
{
}

void Emitter::UpdateParticle(float currentTime, int i)
{
}

void Emitter::SpawnParticle(float currentTime)
{
}
