#include "Entity.h"

Entity::Entity(std::shared_ptr<Mesh> mesh) : mesh(mesh)
{
}

Entity::~Entity()
{
}

std::shared_ptr<Mesh> Entity::GetMesh()
{
    return mesh;
}

Transform* Entity::GetTransform()
{
    return &transform;
}

void Entity::SetMesh()
{
    this->mesh = mesh;
}
