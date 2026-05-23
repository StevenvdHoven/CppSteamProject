#pragma once
#include "Component.h"
#include "Math.h"

class Transform : public Component
{
public:
	Transform();

#ifdef _DEBUG
	virtual void ImGui_OnWindow() {}
#endif // _DEBUG


private:
	Math::Vector2 LocalPosition;
	float LocalRotation;
	float LocalScale;
};

