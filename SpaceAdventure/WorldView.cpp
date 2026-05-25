#include "WorldView.h"


void WorldView::ImGui_OnWindow()
{
	ImGui::Begin("World View");
	const Scene& scene = SceneManager::GetCurrentScene();

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
		const auto& gameObjects = scene.GetGameObjects();

		for (const auto& gameObject : gameObjects)
		{
			ImGui_GameObjectHelper(gameObject);
		}

		ImGui::TreePop();
	}


	
	ImGui::End();
}

void WorldView::ImGui_GameObjectHelper(const std::unique_ptr<GameObject>& gameObject)
{
	ImGui::PushID(gameObject.get());
	bool opened = ImGui::TreeNodeEx(gameObject->Name.c_str());
	bool selected = ImGui::IsItemClicked();
	if (selected) {
		OnSelect(gameObject.get());
	}

	// GameObject popup
	if (ImGui::BeginPopupContextItem("GameObjectPopup"))
	{
		if (ImGui::MenuItem("Add Child"))
		{
			GameObject::Instantiate();
		}

		ImGui::EndPopup();
	}

	if (opened)
	{
		// Render children here

		ImGui::TreePop();
	}


	ImGui::PopID();
}

void WorldView::OnSelect(GameObject* gameObject)
{
	SelectedGameObject = gameObject;
}
