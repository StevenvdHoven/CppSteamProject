#include "RenderComponent.h"
#include "Renderer.h"
#include <imgui/imgui.h>

RenderComponent::RenderComponent() :
	Order{0}
{
	Renderer::Register(this);
}

RenderComponent::~RenderComponent()
{
	Renderer::UnRegister(this);
}

void RenderComponent::Load(std::ifstream& inputstream)
{
	inputstream.read(reinterpret_cast<char*>(&Order), sizeof(Order));
}

void RenderComponent::Save(std::ofstream& outputstream)
{
	outputstream.write(reinterpret_cast<const char*>(&Order), sizeof(Order));
}

#ifdef _DEBUG
void RenderComponent::ImGui_OnWindow()
{
	ImGui::InputInt("Order", &Order);
}
#endif