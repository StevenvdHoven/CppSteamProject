#include "SceneManager.h"
#include "Scene.h"
#include "AssetManager.h"
#include "Camera.h"

#ifdef _DEBUG
#include "EditorWindowManager.h"
#include "WorldView.h"
#endif // _DEBUG


SceneManager* SceneManager::Instance = nullptr;

SceneManager::SceneManager()
{
	Scene untitledScene{};
	Asset* assetScene = (Asset*)&untitledScene;
	assetScene->Name = "Untitled";
	
	LoadScene(untitledScene);

	auto camera = std::make_unique<GameObject>();
	camera->Name = "Camera";
	camera->AddComponent<Camera>();

	CurrentScene.GameObjects.emplace_back(std::move(camera));
}

void SceneManager::LoadScene(Scene& scene)
{
	CurrentScene = std::move(scene);
	std::ifstream inputstream;
	inputstream.open(CurrentScene.GetPath(), std::ios::binary | std::ios::in);
	if (!inputstream.is_open() || inputstream.bad())
	{
		return;
	}
	CurrentScene.Load(inputstream);
	inputstream.close();

#ifdef _DEBUG
	EditorWindowManager::GetChecked().GetEditorWindow<WorldView>().UnSelect();
#endif // _DEBUG

}

void SceneManager::Start()
{
	CurrentScene.Start();
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

void SceneManager::SaveCurrentScene()
{
	AssetManager::SaveAsset(&Instance->CurrentScene);
}

void SceneManager::UpdateScene(float deltaTime)
{
	if (Instance) {
		Instance->Update(deltaTime);
	}
}

void SceneManager::OpenScene(Scene& scene)
{
	if (Instance) {
		Instance->LoadScene(scene);
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

void SceneManager::StartScene()
{
	Instance->Start();
}
