#include "SceneManager.h"
#include "Scene.h"

SceneManager* SceneManager::Instance = nullptr;

SceneManager::SceneManager()
{
	Scene untitledScene{};
	Asset* assetScene = (Asset*)&untitledScene;
	assetScene->Name = "Untitled";
	
	LoadScene(untitledScene);
}

void SceneManager::LoadScene(Scene& scene)
{
	CurrentScene = std::move(scene);
}

void SceneManager::Update(float deltaTime)
{
	CurrentScene.Update(deltaTime);
}

void SceneManager::Init()
{
	if (Instance) {
		return;
	}
	Instance = new SceneManager();
}

void SceneManager::CleanUp()
{
	if (Instance) {
		delete Instance;
	}
}

void SceneManager::UpdateScene(float deltaTime)
{
	if (Instance) {
		Instance->Update(deltaTime);
	}
}

void SceneManager::AddGameObject(std::unique_ptr<GameObject>&& gameObject)
{
	Instance->CurrentScene.Add(std::move(gameObject));
}

void SceneManager::DeleteGameObject(GameObject* gameObject)
{
	Instance->CurrentScene.Delete(gameObject);
}
