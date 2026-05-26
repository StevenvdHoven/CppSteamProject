#ifdef _DEBUG
#include "WorldView.h"

void WorldView::ImGui_OnWindow()
{
	ImGui::Begin("World View");
	const Scene& scene = SceneManager::GetCurrentScene();

	if (ImGui::BeginPopupContextItem("WorldView Popup",ImGuiPopupFlags_NoOpenOverItems)) {
		// None
	}

	if (ImGui::IsKeyDown(ImGuiKey_S) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
		SceneManager::SaveCurrentScene();
	}


	bool opened = ImGui::TreeNode(scene.GetName().c_str());

	// Scene node popup
	if (ImGui::BeginPopupContextItem("ScenePopup"))
	{
		if (ImGui::MenuItem("Add GameObject"))
		{
			GameObject::Instantiate();
		}

		ImGui::EndPopup();
	}

	if (opened)
	{
		auto& gameObjects = scene.GetGameObjects();
		size_t sizeOfGameObjects = gameObjects.size();

		for (int index{0}; index < sizeOfGameObjects; ++index)
		{
			const auto& gameObject = gameObjects[index];

			if (gameObject == nullptr) {
				continue;
			}

			if (gameObject->GetParent() == nullptr) {
				ImGui_GameObjectHelper(gameObject.get());
			}
		}

		ImGui::TreePop();
	}


	
	ImGui::End();
}

void WorldView::ImGui_GameObjectHelper(GameObject* gameObject)
{
	ImGui::PushID(gameObject);
	bool opened = ImGui::TreeNodeEx(gameObject->Name.c_str());
	bool selected = ImGui::IsItemClicked();
	if (selected) {
		OnSelect(gameObject);
	}

	// GameObject popup
	if (ImGui::BeginPopupContextItem("GameObjectPopup"))
	{
		if (ImGui::MenuItem("Add Child"))
		{
			GameObject* child = GameObject::Instantiate();
			child->SetParent(gameObject);
		}

		ImGui::EndPopup();
	}

	if (opened)
	{
		auto gameObjects = gameObject->GetChilderen();

		for (GameObject* child : gameObjects)
		{
			ImGui_GameObjectHelper(child);
		}
		ImGui::TreePop();
	}


	ImGui::PopID();
}

void WorldView::OnSelect(GameObject* gameObject)
{
	SelectedGameObject = gameObject;
}
#endif