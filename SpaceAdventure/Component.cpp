#include "Component.h"
#include "GameObject.h"
#include "SceneManager.h"

void Component::Destroy(GameObject* gameObject)
{
	SceneManager::DeleteGameObject(gameObject);
}

GameObject* Component::GetParent() const
{
	return Owner->GetParent();
}

Transform* Component::GetTransform()
{
	return Owner->GetTransform();
}
