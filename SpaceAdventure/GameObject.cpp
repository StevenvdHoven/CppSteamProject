#include "GameObject.h"
#include "Component.h"

void GameObject::Start()
{
}

void GameObject::Update(float deltaTime)
{
	for (auto& component : Components)
	{
		component->Update(deltaTime);
	}
}

void GameObject::OnDestroy()
{
}
