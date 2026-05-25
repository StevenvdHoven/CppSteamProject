#pragma once
#include <iostream>
#include "Scene.h"

class SceneManager
{
public:
	SceneManager();

	void LoadScene(Scene& scene);
	
	void Start();
	void Update(float deltaTime);

private:
	Scene CurrentScene;

public:
	static void Init();
	static void CleanUp();

	static void SaveCurrentScene();
	static void UpdateScene(float deltaTime);
	static void OpenScene(Scene& scene);
	
	static const Scene& GetCurrentScene() {
		if (Instance) {
			return Instance->CurrentScene;
		}
	}
	static void AddGameObject(std::unique_ptr<GameObject>&& gameObject);
	static void DeleteGameObject(GameObject* gameObject);

private:
	static SceneManager* Instance;
};

