#pragma once
#include <memory>
#include "Mesh.h"
#include "Transform.h"

class Entity
{
public:
	Entity(std::shared_ptr<Mesh> mesh);
	~Entity();

	std::shared_ptr<Mesh> GetMesh();
	Transform* GetTransform();

	void SetMesh();


private:
	std::shared_ptr<Mesh> mesh;
	Transform transform;
};

