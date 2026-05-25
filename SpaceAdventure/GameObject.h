#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "Component.h"
class GameObject
{
public:
	GameObject();
	virtual ~GameObject() = default;

	void Start();
	void Update(float deltaTime);
	void OnDestroy();

	template<typename T, typename... Args>
	T& AddComponent(Args&& ... args) {
		auto uniqueComp = std::make_unique<T>(std::forward<Args>(args)...);
		T& refComp = *uniqueComp.get();
		Components.emplace_back(std::move(uniqueComp));
		return refComp;
	}

	template<typename T>
	T& AddComponent(std::unique_ptr<T>&& component)
	{
		T* ptr = component.get();
		Components.emplace_back(std::move(component));
		return *ptr;
	}

#ifdef _DEBUG
	virtual void ImGui_OnInpect();
#endif // _DEBUG

	static GameObject* Instantiate();

public:
	std::string Name;

private:
	uint32_t Id;

	using ComponentVector = std::vector<std::unique_ptr<Component>>;
	ComponentVector Components;
};
