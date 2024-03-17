#pragma once
#include <memory>
#include "Mesh.h"
#include "Transform.h"
#include "Material.h"

class Entity
{
public:
	Entity(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material);
	~Entity();

	std::shared_ptr<Mesh> GetMesh();
	Transform* GetTransform();
	std::shared_ptr<Material> GetMaterial();


	void SetMesh();
	void SetMaterial();

private:
	std::shared_ptr<Mesh> mesh;
	Transform transform;
	std::shared_ptr<Material> material;
};

