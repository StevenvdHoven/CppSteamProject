#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "Component.h"
#include <fstream>

class Transform;

class GameObject
{
	using ComponentVector = std::vector<std::unique_ptr<Component>>;
	using GameObjectVector = std::vector<GameObject*>;
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

	void RemoveComponent(Component* component) {
		auto compIt = std::find_if(Components.begin(), Components.end(), [component](std::unique_ptr<Component>& other) {
			return other.get() == component;
		});

		if (compIt != Components.end()) {
			Components.erase(compIt);
		}
	}

	void SetParent(GameObject* parent, bool keepWorldPos = true);
	GameObject* GetParent() const { return Parent; }
	
	Transform* GetTransform() { return TransformPtr; }

	
	const GameObjectVector GetChilderen() const { return Childeren; }

#ifdef _DEBUG
	virtual void ImGui_OnInpect();
#endif // _DEBUG

	static GameObject* Instantiate();

private:
	void AddChild(GameObject* child);
	void RemoveChild(GameObject* child);

public:
	std::string Name;

private:
	uint32_t Id;
	
	GameObject* Parent = nullptr;
	Transform* TransformPtr = nullptr;

	ComponentVector Components;
	GameObjectVector Childeren;
};
