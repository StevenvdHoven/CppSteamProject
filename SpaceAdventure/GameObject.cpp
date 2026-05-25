#include "GameObject.h"
#include "Component.h"
#include "SceneManager.h"
#include <imgui/imgui.h>

GameObject::GameObject()
{
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