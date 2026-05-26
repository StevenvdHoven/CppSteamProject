#include "Component.h"
#include "GameObject.h"
#include "SceneManager.h"

void Component::Destroy(GameObject* gameObject)
{
	SceneManager::DeleteGameObject(gameObject);
}

Transform* Component::GetTransform()
{
	return Owner->GetTransform();
}
