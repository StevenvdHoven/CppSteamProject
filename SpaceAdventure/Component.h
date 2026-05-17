#pragma once

class GameObject;

class Component
{
public:
	virtual void Start() {}
	virtual void Update(float deltaTime) {}
	virtual void OnDestroy() {}

private:
	GameObject* Owner;
};

