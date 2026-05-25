#include "GameObject.h"
#include "Component.h"
#include "SceneManager.h"
#include "BinarySaving.h"
#include <imgui/imgui.h>
#include "Transform.h"

GameObject::GameObject():
	Name{"GameObject"},
	Id{0}
{
	TransformPtr = AddComponent<Transform>();
}

void GameObject::Load(std::ifstream& inputstream)
{
	Components.clear();
	Name = BinarySaving::ReadString(inputstream);

	std::uint32_t componentsSize = 0;
	inputstream.read(reinterpret_cast<char*>(&componentsSize), sizeof(componentsSize));
	for (int compIndex{ 0 }; compIndex < componentsSize; ++compIndex)
	{
		std::string compName = BinarySaving::ReadString(inputstream);
		auto comp = ComponentRegistery::Create(compName);
		comp->Load(inputstream);

		if (compName == Transform::CompType) {
			TransformPtr = dynamic_cast<Transform*>(comp.get());
		}
		AddComponent(std::move(comp));
	}
}

void GameObject::Save(std::ofstream& outputstream)
{
	BinarySaving::Write(outputstream, Name);

	std::uint32_t componentsSize = static_cast<std::uint32_t>(Components.size());
	outputstream.write(reinterpret_cast<const char*>(&componentsSize), sizeof(componentsSize));
	for (int compIndex{ 0 }; compIndex < componentsSize; ++compIndex)
	{
		if (!Components[compIndex]->GetName().size()) {
			continue;
		}
		BinarySaving::Write(outputstream, Components[compIndex]->GetName());
		Components[compIndex]->Save(outputstream);
	}
}

void GameObject::Start()
{
}

void GameObject::Update(float deltaTime)
{
	for (auto& component : Components)
	{
		component->Update(deltaTime);
	}
}

void GameObject::OnDestroy()
{
}

#ifdef _DEBUG
void GameObject::ImGui_OnInpect()
{
	char buffer[256]{};

	strncpy(buffer, Name.c_str(), sizeof(buffer) - 1);

	if (ImGui::InputText("##Name", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		Name = buffer;
	}
	ImGui::Text("ID: %i", Id);

	for (auto& component : Components)
	{
		if(ImGui::CollapsingHeader(component->GetName().c_str(),ImGuiTreeNodeFlags_DefaultOpen)) {
			component->ImGui_OnWindow();
		}
	}

	ImGui::Separator();
	std::vector<std::string> componentTypes = ComponentRegistery::GetAvailableComponentTypes();
	static std::string popupname = "Add Component Popup";

	if (ImGui::Button("Add Component"))
	{
		ImGui::OpenPopup(popupname.c_str());
	}

	if (ImGui::BeginPopup(popupname.c_str(), ImGuiWindowFlags_Popup))
	{
		static int item_selected_idx = 0;
		for (int n = 0; n < componentTypes.size(); n++)
		{
			const bool is_selected = (item_selected_idx == n);
			if (ImGui::Selectable(componentTypes[n].c_str(), is_selected)) {
				item_selected_idx = n;
				AddComponent(std::move(ComponentRegistery::Create(componentTypes[n])));
				ImGui::CloseCurrentPopup();
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndPopup();
	}
}
#endif // _DEBUG

GameObject* GameObject::Instantiate()
{
	auto newGameObj = std::make_unique<GameObject>();
	newGameObj->Name = "GameObject";
	auto gameObjPtr = newGameObj.get();
	SceneManager::AddGameObject(std::move(newGameObj));
	return gameObjPtr;
}