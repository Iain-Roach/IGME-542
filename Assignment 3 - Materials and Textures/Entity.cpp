#include "Entity.h"

Entity::Entity(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material) : mesh(mesh), material(material)
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

std::shared_ptr<Material> Entity::GetMaterial()
{
    return material;
}

void Entity::SetMesh()
{
    this->mesh = mesh;
}

void Entity::SetMaterial()
{
    this->material = material;
}
