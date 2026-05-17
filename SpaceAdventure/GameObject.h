#pragma once
#include <iostream>
#include <vector>

class Component;

class GameObject
{
public:
	void Start();
	void Update(float deltaTime);
	void OnDestroy();

private:
	using ComponentVector = std::vector<std::unique_ptr<Component>>;
	ComponentVector Components;
};

