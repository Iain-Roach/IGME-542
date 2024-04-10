#include "Emitter.h"

using namespace DirectX;

Emitter::Emitter(
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	std::shared_ptr<Material> material,
	int maxParticles,
	int particlesPerSecond,
	float lifetime,
	float startSize,
	float endSize,
	DirectX::XMFLOAT4 startColor,
	DirectX::XMFLOAT4 endColor,
	bool isBox,
	DirectX::XMFLOAT3 emitterPosition,
	DirectX::XMFLOAT3 startVelocity,
	DirectX::XMFLOAT3 acceleration) 
	:
	device(device),
	material(material),
	maxParticles(maxParticles),
	particlesPerSecond(particlesPerSecond),
	lifetime(lifetime),
	startSize(startSize),
	endSize(endSize),
	startColor(startColor),
	endColor(endColor),
	isBox(isBox),
	startVelocity(startVelocity),
	acceleration(acceleration),
	particles(0)
{
	secondsPerParticle = 1.0f / particlesPerSecond;

	timeSinceLastEmit = 0.0f;
	numLiving = 0;
	firstAliveIndex = 0;
	firstDeadIndex = 0;

	this->transform.SetPosition(emitterPosition);

	
	if (particles) delete[] particles;
	indexBuffer.Reset();
	particleDataBuffer.Reset();
	particleDataSRV.Reset();

	
	particles = new Particle[maxParticles];
	ZeroMemory(particles, sizeof(Particle) * maxParticles);

	
	int numIndices = maxParticles * 6;
	unsigned int* indices = new unsigned int[numIndices];
	int indexCount = 0;
	for (int i = 0; i < maxParticles * 4; i += 4)
	{
		indices[indexCount++] = i;
		indices[indexCount++] = i + 1;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i + 3;
	}
	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices;

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = 0;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(unsigned int) * maxParticles * 6;
	device->CreateBuffer(&ibDesc, &indexData, indexBuffer.GetAddressOf());
	delete[] indices; 

	
	D3D11_BUFFER_DESC allParticleBufferDesc = {};
	allParticleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	allParticleBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	allParticleBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	allParticleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	allParticleBufferDesc.StructureByteStride = sizeof(Particle);
	allParticleBufferDesc.ByteWidth = sizeof(Particle) * maxParticles;
	device->CreateBuffer(&allParticleBufferDesc, 0, particleDataBuffer.GetAddressOf());

	
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = maxParticles;
	device->CreateShaderResourceView(particleDataBuffer.Get(), &srvDesc, particleDataSRV.GetAddressOf());
}

Emitter::~Emitter()
{
	delete[] particles;
}

Transform* Emitter::GetTransform() 
{ 
	return &transform; 
}
std::shared_ptr<Material> Emitter::GetMaterial() 
{ 
	return material; 
}


void Emitter::Update(float dt, float currentTime)
{
	
	if (numLiving > 0)
	{
		
		if (firstAliveIndex < firstDeadIndex)
		{
			
			for (int i = firstAliveIndex; i < firstDeadIndex; i++)
				UpdateParticle(currentTime, i);
		}
		else if (firstDeadIndex < firstAliveIndex)
		{
			
			for (int i = firstAliveIndex; i < maxParticles; i++)
				UpdateParticle(currentTime, i);

			
			for (int i = 0; i < firstDeadIndex; i++)
				UpdateParticle(currentTime, i);
		}
		else
		{
			
			for (int i = 0; i < maxParticles; i++)
				UpdateParticle(currentTime, i);
		}
	}

	
	timeSinceLastEmit += dt;

	while (timeSinceLastEmit > secondsPerParticle)
	{
		SpawnParticle(currentTime);
		timeSinceLastEmit -= secondsPerParticle;
	}
}


void Emitter::UpdateParticle(float currentTime, int index)
{
	float age = currentTime - particles[index].EmitTime;

	
	if (age >= lifetime)
	{
		
		firstAliveIndex++;
		firstAliveIndex %= maxParticles;
		numLiving--;
	}
}

void Emitter::SpawnParticle(float currentTime)
{
	if (numLiving == maxParticles)
		return;

	int spawnIndex = firstDeadIndex;

	particles[spawnIndex].EmitTime = currentTime;
	particles[spawnIndex].StartPosition = transform.GetPosition();

	// if is box then the start position needs to be moved randomly in a box
	if (isBox)
	{
		int max = 2;
		int min = -2;
		particles[spawnIndex].StartPosition.x += ((float)rand() / RAND_MAX * (max - min) + min);
		particles[spawnIndex].StartPosition.y += ((float)rand() / RAND_MAX * (max - min) + min);
		particles[spawnIndex].StartPosition.z += ((float)rand() / RAND_MAX * (max - min) + min);
	}
	

	
	particles[spawnIndex].StartVelocity = startVelocity;

	firstDeadIndex++;
	firstDeadIndex %= maxParticles; 

	numLiving++;
}

void Emitter::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera, float currentTime)
{
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	context->Map(particleDataBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	if (firstAliveIndex < firstDeadIndex)
	{
		memcpy(
			mapped.pData,
			particles + firstAliveIndex,
			sizeof(Particle) * numLiving);
	}
	else
	{
		memcpy(
			mapped.pData, 
			particles, 
			sizeof(Particle) * firstDeadIndex);
		memcpy(
			(void*)((Particle*)mapped.pData + firstDeadIndex),
			particles + firstAliveIndex, 
			sizeof(Particle) * (maxParticles - firstAliveIndex)); 
	}

	context->Unmap(particleDataBuffer.Get(), 0);

	UINT stride = 0;
	UINT offset = 0;
	ID3D11Buffer* nullBuffer = 0;
	context->IASetVertexBuffers(0, 1, &nullBuffer, &stride, &offset);
	context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	material->PrepareMaterial(&transform, camera);

	std::shared_ptr<SimpleVertexShader> vs = material->GetVertexShader();
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("projection", camera->GetProjection());
	vs->SetFloat("currentTime", currentTime);
	vs->SetFloat("lifetime", lifetime);
	vs->SetFloat3("acceleration", acceleration);
	vs->SetFloat("startSize", startSize);
	vs->SetFloat("endSize", endSize);
	vs->SetFloat4("startColor", startColor);
	vs->SetFloat4("endColor", endColor);
	vs->CopyAllBufferData();

	vs->SetShaderResourceView("ParticleData", particleDataSRV);


	context->DrawIndexed(numLiving * 6, 0, 0);
}



