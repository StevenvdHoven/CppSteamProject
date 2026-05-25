#pragma once
#include "ComponentRegistery.h"
#include <iostream>
#include <string>

class GameObject;
class Transform;

class Component
{
public:
	Component() : Owner{ nullptr } {}
	virtual ~Component() = default;

	virtual void Load(std::ifstream& inputstream) {}
	virtual void Save(std::ofstream& outputstream) {}

	virtual void Start() {}
	virtual void Update(float deltaTime) {}
	virtual void OnDestroy() {}

	Transform* GetTransform();

#ifdef _DEBUG
	virtual void ImGui_OnWindow() {}
#endif // _DEBUG

	virtual const std::string GetName() const { return "Component"; }

private:
	GameObject* Owner;
	friend GameObject;
};

