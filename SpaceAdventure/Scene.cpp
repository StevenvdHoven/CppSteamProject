#include "Scene.h"

void Scene::Update(float deltaTime)
{
	for (auto& gameObject : GameObjects) {
		gameObject->Update(deltaTime);
	}
}
