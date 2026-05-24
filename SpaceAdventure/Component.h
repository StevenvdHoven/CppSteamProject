#pragma once
#include "ComponentRegistery.h"
#include <string>

class GameObject;

class Component
{
public:
	Component() : Owner{ nullptr } {}
	virtual ~Component() = default;

	virtual void Start() {}
	virtual void Update(float deltaTime) {}
	virtual void OnDestroy() {}

#ifdef _DEBUG
	virtual void ImGui_OnWindow() {}
#endif // _DEBUG

	virtual const std::string GetName() const { return "Component"; }

private:
	GameObject* Owner;
};

