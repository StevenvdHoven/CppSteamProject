#pragma once
#include <iostream>
#include <vector>
#include "GameObject.h"
#include "Asset.h"

class GameObject;
class SceneManager;

class Scene : public Asset
{
	using GameObjectVector = std::vector<std::unique_ptr<GameObject>>;
public:
	ASSET_LOOKUP(Scene, "scene")

	virtual void Load(std::ifstream& inputstream) override;
	virtual void Save(std::ofstream& outputstream) override;

	void Start();
	void Update(float deltaTime);
	void OnDestroy();

#ifdef _DEBUG
	void ImGui_ContentHelper() override;
#endif // _DEBUG

	const GameObjectVector& GetGameObjects() const { return GameObjects; }

private:
	void Add(std::unique_ptr<GameObject>&& gameObject)
	{
		GameObjects.emplace_back(std::move(gameObject));
	}

	void Delete(GameObject* gameObject)
	{
		auto pos = std::find_if(GameObjects.begin(),GameObjects.end(), [&gameObject](std::unique_ptr<GameObject>& obj)
			{
				return obj.get() == gameObject;
			});
		if (pos == GameObjects.end()) {
			return;
		}
		GameObjects.erase(pos);
	}

private:
	GameObjectVector GameObjects;
	friend SceneManager;
};

