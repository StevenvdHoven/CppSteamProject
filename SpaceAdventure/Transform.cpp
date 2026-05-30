#include "Transform.h"
#include <imgui/imgui.h>
#include "GameObject.h"

Transform::Transform() :
	LocalPosition{},
	WorldPosition{},
	LocalRotation{},
	WorldRotation{},
	LocalScale{1.0f},
	WorldScale{LocalScale}
{
	DirtyFlags.set();
}

void Transform::Load(std::ifstream& inputstream)
{
	inputstream.read(reinterpret_cast<char*>(&LocalPosition), sizeof(LocalPosition));
	inputstream.read(reinterpret_cast<char*>(&LocalRotation), sizeof(LocalRotation));
	inputstream.read(reinterpret_cast<char*>(&LocalScale), sizeof(LocalScale));
}

void Transform::Save(std::ofstream& outputstream)
{
	outputstream.write(reinterpret_cast<const char*>(&LocalPosition), sizeof(LocalPosition));
	outputstream.write(reinterpret_cast<const char*>(&LocalRotation), sizeof(LocalRotation));
	outputstream.write(reinterpret_cast<const char*>(&LocalScale), sizeof(LocalScale));
}

void Transform::Update(float deltaTime)
{
	if (IsDirty(DirtyFlag::POSITION)) {
		CalculateWorldPosition();
	}

	if (IsDirty(DirtyFlag::ROTATION)) {
		CalculateWorldRotation();
	}

	if (IsDirty(DirtyFlag::SCALE)) {
		CalculateWorldScale();
	}

}

void Transform::SetLocalPosition(const Math::Vector2& localPos)
{
	if (LocalPosition != localPos) {
		SetDirty(DirtyFlag::POSITION);
		LocalPosition = localPos;
	}
}

void Transform::SetLocalRotation(float localRot)
{
	if (LocalRotation != localRot) {
		SetDirty(DirtyFlag::POSITION, true);
		SetDirty(DirtyFlag::ROTATION);
		LocalRotation = localRot;
	}
}

void Transform::SetLocalScale(float localScale)
{
	if (LocalScale != localScale) {

		SetDirty(DirtyFlag::POSITION, true);
		SetDirty(DirtyFlag::SCALE);
		LocalScale = localScale;
	}
}

Math::Vector2 Transform::GetWorldLocation()
{
	if (IsDirty(DirtyFlag::POSITION)) {
		CalculateWorldPosition();
	}
	return WorldPosition;
}

void Transform::SetWorldLocation(const Math::Vector2& worldPos)
{
	WorldPosition = worldPos;
	if (GameObject* parent = GetParent()) {
		LocalPosition = WorldPosition - GetParent()->GetTransform()->GetWorldLocation();
	}
	else {
		LocalPosition = WorldPosition;
	}

	auto childeren = GetOwner()->GetChilderen();
	for (auto child : childeren) {
		child->GetTransform()->SetDirty(DirtyFlag::POSITION);
	}
}

float Transform::GetWorldRotation()
{
	if (IsDirty(DirtyFlag::ROTATION)) {
		CalculateWorldRotation();
	}
	return WorldRotation;
}

void Transform::SetWorldRotation(float degrees)
{
	WorldRotation = degrees;
	if (GameObject* parent = GetParent()) {
		LocalRotation = WorldRotation - GetParent()->GetTransform()->GetWorldRotation();
	}
	else {
		LocalRotation = WorldRotation;
	}

	auto childeren = GetOwner()->GetChilderen();
	for (auto child : childeren) {
		child->GetTransform()->SetDirty(DirtyFlag::POSITION);
		child->GetTransform()->SetDirty(DirtyFlag::ROTATION);
	}
}

float Transform::GetWorldScale()
{
	if (IsDirty(DirtyFlag::SCALE)) {
		CalculateWorldScale();
	}
	return WorldScale;
}

void Transform::SetWorldScale(float scale)
{
	WorldScale = scale;
	if (GameObject* parent = GetParent()) {
		float parentWorldScale = GetParent()->GetTransform()->GetWorldScale();
		LocalScale = parentWorldScale > 0.0f ? WorldScale / parentWorldScale : 0.0f;
	}
	else {
		LocalScale = WorldScale;
	}

	auto childeren = GetOwner()->GetChilderen();
	for (auto child : childeren) {
		child->GetTransform()->SetDirty(DirtyFlag::POSITION);
		child->GetTransform()->SetDirty(DirtyFlag::SCALE);
	}
}

void Transform::SetDirty(DirtyFlag flag, bool childOnly)
{
	if (flag == DirtyFlag::ALL) {
		SetDirty(DirtyFlag::POSITION, childOnly);
		SetDirty(DirtyFlag::ROTATION, childOnly);
		SetDirty(DirtyFlag::SCALE, childOnly);
		return;
	}

	if (!childOnly) {
		DirtyFlags.set(static_cast<int>(flag), true);
	}

	auto childeren = GetOwner()->GetChilderen();
	for (auto child : childeren) {
		child->GetTransform()->SetDirty(flag);
	}
}

bool Transform::IsDirty(DirtyFlag flag) const
{
	return DirtyFlags.test(static_cast<int>(flag));
}

Math::Matrix2x3 Transform::GetMatrix() const
{
	auto localMatrix = Math::Matrix2x3::FromTranslation(LocalPosition) * Math::Matrix2x3::FromScale(LocalScale) * Math::Matrix2x3::FromRotation(LocalRotation);
	if (GameObject* parent = GetParent()) {
		return parent->GetTransform()->GetMatrix() * localMatrix;
	}
	return localMatrix;
}

void Transform::ClearDirty(DirtyFlag flag)
{
	DirtyFlags.set(static_cast<int>(flag), false);
}

void Transform::CalculateWorldPosition()
{
	if (GameObject* parent = GetParent()) {
		Math::Matrix2x3 parentMatrix = parent->GetTransform()->GetMatrix();
		WorldPosition = parentMatrix.Transform(LocalPosition);
	}
	else {
		WorldPosition = LocalPosition;
	}
	ClearDirty(DirtyFlag::POSITION);
}

void Transform::CalculateWorldRotation()
{
	if (GameObject* parent = GetParent()) {
		WorldRotation = parent->GetTransform()->GetWorldRotation() + LocalRotation;
	}
	else {
		WorldRotation = LocalRotation;
	}
	ClearDirty(DirtyFlag::ROTATION);
}

void Transform::CalculateWorldScale()
{
	if (GameObject* parent = GetParent()) {
		WorldScale = parent->GetTransform()->GetWorldScale() * LocalScale;
	}
	else {
		WorldScale = LocalScale;
	}
	ClearDirty(DirtyFlag::SCALE);
}

#ifdef _DEBUG
void Transform::ImGui_OnWindow()
{
	Math::Vector2 locPos = LocalPosition;
	ImGui::InputFloat2("Local Position", &locPos.x);
	SetLocalPosition(locPos);

	float locRot = LocalRotation;
	ImGui::InputFloat("Local Rotation", &locRot);
	SetLocalRotation(locRot);

	float locScale = LocalScale;
	ImGui::InputFloat("Local Scale", &locScale);
	SetLocalScale(locScale);

	ImGui::Separator();
	if (ImGui::TreeNode("Childeren")) {
		auto childeren = GetOwner()->GetChilderen();
		for (auto child : childeren) {
			ImGui::Text(child->Name.c_str());
		}
		ImGui::TreePop();
	}

}
#endif // _DEBUG