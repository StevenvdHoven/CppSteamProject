#pragma once
#include "Component.h"
#include "Math.h"

class Transform : public Component
{
public:
	DECLARE_COMPONENT(Transform, "Transform")

	Transform();
	virtual ~Transform() = default;

	void Update(float deltaTime) override;

	Math::Vector2 GetLocalPosition() const;
	void SetLocalPosition(const Math::Vector2& localPos);

	float GetLocalRotation() const;
	void SetLocalRotation(float localRot);

	float GetLocalScale() const;
	void SetLocalScale(float localScale);

#ifdef _DEBUG
	virtual void ImGui_OnWindow();
#endif // _DEBUG

private:
	Math::Vector2 LocalPosition;
	float LocalRotation;
	float LocalScale;

	GameObject* Parent;
};

