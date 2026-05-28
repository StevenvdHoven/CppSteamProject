#pragma once
#include "Component.h"
#include "Math.h"
#include <bitset>

class Transform : public Component
{
	enum struct DirtyFlag : uint8_t {
		POSITION,
		ROTATION,
		SCALE
	};
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

	Math::Vector2 GetWorldLocation();
	void SetWorldLocation(const Math::Vector2& worldPos);

	float GetWorldRotation();
	void SetWorldRotation(float degrees);

	float GetWorldScale();
	void SetWorldScale(float scale);

	void SetDirty(DirtyFlag flag, bool childOnly = false);
	bool IsDirty(DirtyFlag flag) const;

	Math::Matrix2x3 GetMatrix() const {
		return  Math::Matrix2x3::FromTranslation(LocalPosition) * Math::Matrix2x3::FromScale(LocalScale) * Math::Matrix2x3::FromRotation(LocalRotation);
	}

private:
	void ClearDirty(DirtyFlag flag);
	void CalculateWorldPosition();
	void CalculateWorldRotation();
	void CalculateWorldScale();


public:
#ifdef _DEBUG
	virtual void ImGui_OnWindow();
#endif // _DEBUG

private:
	std::bitset<3> DirtyFlags;

	Math::Vector2 LocalPosition;
	Math::Vector2 WorldPosition;
	float LocalRotation;
	float WorldRotation;
	float LocalScale;
	float WorldScale;

	friend GameObject;
};

