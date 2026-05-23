#pragma once
class GameObject;

class Component
{
public:
	virtual void Start() {}
	virtual void Update(float deltaTime) {}
	virtual void OnDestroy() {}

#ifdef _DEBUG
	virtual void ImGui_OnWindow() {}
#endif // _DEBUG

private:
	GameObject* Owner;
};

