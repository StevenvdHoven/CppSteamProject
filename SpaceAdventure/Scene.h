#pragma once
#include <iostream>
#include <vector>
#include "Asset.h"

class GameObject;

class Scene : public Asset
{
	using GameObjectVector = std::vector<std::unique_ptr<GameObject>>;
public:
	ASSET_LOOKUP(Scene, "scene")

	void Start();
	void Update(float deltaTime);
	void OnDestroy();

	const GameObjectVector& GetGameObjects() const { return GameObjects; }
private:
	GameObjectVector GameObjects;
};

