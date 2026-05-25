#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "Component.h"
#include <fstream>

class Transform;

class GameObject
{
public:
	GameObject();
	virtual ~GameObject() = default;

	virtual void Load(std::ifstream& inputstream);
	virtual void Save(std::ofstream& outputstream);

	void Start();
	void Update(float deltaTime);
	void OnDestroy();

	template<typename T, typename... Args>
		requires std::is_base_of_v<Component, T>
	T* AddComponent(Args&& ... args) {
		auto uniqueComp = std::make_unique<T>(std::forward<Args>(args)...);
		T* refComp = uniqueComp.get();
		Component* comp = (Component*)refComp;
		comp->Owner = this;
		Components.emplace_back(std::move(uniqueComp));
		return refComp;
	}

	template<typename T>
		requires std::is_base_of_v<Component, T>
	T* AddComponent(std::unique_ptr<T>&& component)
	{
		T* ptr = component.get();
		Component* comp = (Component*)ptr;
		comp->Owner = this;
		Components.emplace_back(std::move(component));
		return ptr;
	}

	template<typename ComponentType>
	requires std::is_base_of_v<Component, ComponentType>
	ComponentType* GetComponent()
	{
		for (const auto& component : Components) {
			ComponentType* comp = dynamic_cast<ComponentType*>(component.get());
			if (comp) {
				return comp;
			}
		}
		return nullptr;
	}

	Transform* GetTransform() { return TransformPtr; }

#ifdef _DEBUG
	virtual void ImGui_OnInpect();
#endif // _DEBUG

	static GameObject* Instantiate();

public:
	std::string Name;

private:
	uint32_t Id;

	Transform* TransformPtr;

	using ComponentVector = std::vector<std::unique_ptr<Component>>;
	ComponentVector Components;
};
