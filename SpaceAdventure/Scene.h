#pragma once
#include <iostream>
#include <vector>
#include "Asset.h"

class GameObject;

class Scene : public Asset
{
public:
	void Start();
	void Update(float deltaTime);
	void OnDestroy();

private:
	using GameObjectVector = std::vector<std::unique_ptr<GameObject>>;
	GameObjectVector GameObjects;
};

