#include "Emitter.h"

Emitter::Emitter(int maxParticles, int particlesPerSecond, float lifetime, Microsoft::WRL::ComPtr<ID3D11Device> device,std::shared_ptr<Material> material, DirectX::XMFLOAT3 startVel, DirectX::XMFLOAT3 acceleration) :
	maxParticles(maxParticles), particlesPerSecond(particlesPerSecond), lifetime(lifetime), device(device),mat(material), startVel(startVel), acceleration(acceleration), particles(0)
{

	secondsPerParticle = 1.0f / particlesPerSecond;

	lastEmitTime = 0.0f;
	numLivingParticles = 0;
	firstLivingParticle = 0;
	firstDeadParticle = 0;


	if (particles) delete[] particles;
	indexBuffer.Reset();
	particleBuffer.Reset();
	particleSRV.Reset();

	particles = new Particle[maxParticles];
	ZeroMemory(particles, sizeof(Particle) * maxParticles);

	int numIndices = maxParticles * 6;
	unsigned int* indices = new unsigned int[numIndices];
	int indexCount = 0;
	for (int i = 0; i < maxParticles * 4; i += 4)
	{
		indices[indexCount++] = i;
		indices[indexCount++] = i  +1;
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
	device->CreateBuffer(&allParticleBufferDesc, 0, particleBuffer.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = maxParticles;
	device->CreateShaderResourceView(particleBuffer.Get(), &srvDesc, particleSRV.GetAddressOf());
}

Emitter::~Emitter()
{
	delete[] particles;
}

void Emitter::Update(float dt, float currentTime)
{
	// do particles exist????
	if (numLivingParticles > 0)
	{
		if (firstLivingParticle < firstDeadParticle)
		{
			for (int i = firstLivingParticle; i < firstDeadParticle; i++)
			{
				//AAhhh
				UpdateParticle(i, currentTime);
			}
		}
		else if (firstDeadParticle < firstLivingParticle)
		{
			for (int i = firstLivingParticle; i < maxParticles; i++)
			{
				UpdateParticle(i, currentTime);
			}
			for (int i = 0; i < firstDeadParticle; i++)
			{
				UpdateParticle(i, currentTime);
			}
		}
		else
		{
			for (int i = 0; i < maxParticles; i++)
			{
				UpdateParticle(i, currentTime);
			}
		}
	}

	lastEmitTime += dt;

	while (lastEmitTime > secondsPerParticle)
	{
		//Eehhh
		SpawnParticle(currentTime);
		lastEmitTime -= secondsPerParticle;
	}
}

void Emitter::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera, float currentTime)
{
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	context->Map(particleBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	if (firstLivingParticle < firstDeadParticle)
	{
		memcpy(mapped.pData, particles + firstLivingParticle, sizeof(Particle) * numLivingParticles);
	}
	else
	{
		memcpy(mapped.pData, particles + firstLivingParticle, sizeof(Particle) * numLivingParticles);
		memcpy((void*)((Particle*)mapped.pData + firstDeadParticle), particles + firstLivingParticle, sizeof(Particle) * (maxParticles - firstLivingParticle));
	}

	context->Unmap(particleBuffer.Get(), 0);



	UINT stride = 0;
	UINT offset = 0;
	ID3D11Buffer* nullBuffer = 0;
	context->IASetVertexBuffers(0, 1, &nullBuffer, &stride, &offset);
	context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	mat->PrepareMaterial(&transform, camera);
	
	std::shared_ptr<SimpleVertexShader> vs = mat->GetVertexShader();
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("proj", camera->GetProjection());

	vs->SetFloat("currentTime", currentTime);
	vs->SetFloat3("acceleration", acceleration);

	vs->SetFloat("lifetime", lifetime);

	vs->CopyAllBufferData();

	vs->SetShaderResourceView("ParticleBuffer", particleSRV);

	context->DrawIndexed(numLivingParticles * 6, 0, 0);
}

void Emitter::UpdateParticle(int index, float currentTime)
{
	float age = currentTime - particles[index].emitTime;
	if (age >= lifetime)
	{
		firstLivingParticle++;
		firstLivingParticle %= maxParticles;
		numLivingParticles--;
	}
}

void Emitter::SpawnParticle(float currentTime)
{
	if (numLivingParticles >= maxParticles)
	{
		return;
	}

	int spawnIndex = firstDeadParticle;

	particles[spawnIndex].emitTime = currentTime;

	particles[spawnIndex].startPos = transform.GetPosition();
	particles[spawnIndex].startVel = startVel;

	firstDeadParticle++;
	firstDeadParticle %= maxParticles;
	numLivingParticles++;
}
