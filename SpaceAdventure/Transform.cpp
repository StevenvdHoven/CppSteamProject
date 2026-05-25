#include "Transform.h"
#include <imgui/imgui.h>

Transform::Transform() :
	LocalPosition{},
	LocalRotation{ 0.0f },
	LocalScale{1.0f}
{
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
}

#ifdef _DEBUG
void Transform::ImGui_OnWindow()
{
	ImGui::InputFloat2("Local Position", &LocalPosition.x);
	ImGui::InputFloat("Local Rotation", &LocalRotation);
	ImGui::InputFloat("Local Scale", &LocalScale);
}
#endif // _DEBUG