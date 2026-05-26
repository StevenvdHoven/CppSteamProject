#pragma once
#include "Component.h"
#include "Math.h"
#include <bitset>

class Transform : public Component
{
public:
	DECLARE_COMPONENT(Transform, "Transform")

	Transform();
	virtual ~Transform() = default;

	virtual void Load(std::ifstream& inputstream) override;
	virtual void Save(std::ofstream& outputstream) override;

	void Update(float deltaTime) override;

	Math::Vector2 GetLocalPosition() const { return LocalPosition; }
	void SetLocalPosition(const Math::Vector2& localPos);

	float GetLocalRotation() const { return LocalRotation; }
	void SetLocalRotation(float localRot);

	float GetLocalScale() const { return LocalScale; }
	void SetLocalScale(float localScale);

#ifdef _DEBUG
	virtual void ImGui_OnWindow();
#endif // _DEBUG

private:
	bool IsWorldPositionDirty() const { return DirtyFlags.test(0); }
	bool IsWorldRotationDirty() const { return DirtyFlags.test(1); }
	bool IsWorldScaleDirty() const { return DirtyFlags.test(2); }

private:
	std::bitset<3> DirtyFlags;

	Math::Vector2 LocalPosition;
	float LocalRotation;
	float LocalScale;

	friend GameObject;
};

