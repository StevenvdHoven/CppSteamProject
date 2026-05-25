#include "Transform.h"
#include <imgui/imgui.h>

Transform::Transform() :
	LocalPosition{},
	LocalRotation{ 0.0f },
	LocalScale{1.0f}
{
}

void Transform::Update(float deltaTime)
{
}

#ifdef _DEBUG
void Transform::ImGui_OnWindow()
{
	ImGui::InputFloat2("Local Position", &LocalPosition.x);
	ImGui::InputFloat("Local Rotation", &LocalRotation);
	ImGui::InputFloat("Local Scale", &LocalScale);
}
#endif // _DEBUG