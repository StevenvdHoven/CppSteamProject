#ifdef _DEBUG
#pragma once
#include "EditorWindow.h"
#include "SceneManager.h"

class GameObject;

class WorldView final : public EditorWindow
{
public:
	WorldView() {}
	 
	GameObject* GetSelectGameObject() { return SelectedGameObject; }

	void ImGui_OnWindow() override;

	void UnSelect() { SelectedGameObject = nullptr; }
private:
	void ImGui_GameObjectHelper(GameObject* gameObject);
	
	void OnSelect(GameObject* gameObject);

private:
	GameObject* SelectedGameObject = nullptr;
};
#endif // _DEBUG
