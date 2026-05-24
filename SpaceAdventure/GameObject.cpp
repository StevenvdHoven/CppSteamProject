#include "GameObject.h"
#include "Component.h"
#include "SceneManager.h"
#include <imgui/imgui.h>

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

		component->ImGui_OnWindow();
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