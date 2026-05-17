#pragma once
#include <iostream>

class Scene;

class SceneManager
{
public:
	void LoadScene();
	
	void Start();
	void Update(float deltaTime);

private:
	std::unique_ptr<Scene> CurrentScene;
};

