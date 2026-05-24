#pragma once
#include "EditorWindow.h"
#include "SceneManager.h"

class GameObject;

class WorldView final : public EditorWindow
{
public:
	WorldView() {}

	void ImGui_OnWindow() override;
private:
	void ImGui_GameObjectHelper(const std::unique_ptr<GameObject>& gameObject);
};

