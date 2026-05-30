#pragma once
#define SDL_MAIN_HANDLED
#include <SDL/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/imgui_impl_sdl2.h>
#include <imgui/imgui_impl_sdlrenderer2.h>

#include "iostream"
#include "string"
#include "steam/steam_api.h"
#include <SDL/SDL_image.h>
#include "Renderer.h"
#include "SceneManager.h"
#include "AssetManager.h"
#include "TextureManager.h"
#include "AssetManager.h"

#ifdef _DEBUG
#include "EditorWindowManager.h"
#include "ContentWindow.h"
#include "Viewport.h"
#include "WorldView.h"
#include "Properties.h"
#include "TextureEditor.h"
#endif 

class GameEngineSettings;

class GameEngine
{
public:
	GameEngine();
	~GameEngine();

	void Load();
	void Save();
	void Run();

	void SimulateGame();
	void StopGame();

	bool GameIsRunning() const { return RunningGame; }
	static GameEngine* GetEngine() { return Instance; }

private:
	SDL_Window* MainWindow;
	SDL_Renderer* MainRenderer;

#ifdef _DEBUG
	EditorWindowManager* WindowManager;
	Viewport* GameViewport;
	WorldView* WorldViewWindow;
#endif

private:
	AssetProperty<Scene> StartScene;
	Math::Vector2 WindowSize;

private:
	bool RunningGame = false;

	static GameEngine* Instance;
	friend GameEngineSettings;
};

