#include "GameEngine.h"
#include "Time.h"
#include "GameEngineSettings.h"
#include <fstream>
#include <filesystem>

GameEngine* GameEngine::Instance = nullptr;

GameEngine::GameEngine():
	WindowSize(1800, 900)
{
	Instance = this;
	SteamAPI_Init();

#ifdef _DEBUG
	WindowManager = new EditorWindowManager();
#endif

	

	SDL_Init(SDL_INIT_EVERYTHING);
#ifdef _DEBUG
	MainWindow = SDL_CreateWindow("Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1800, 900, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

#else
	MainWindow = SDL_CreateWindow("CustomGame", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, static_cast<int>(WindowSize.x), static_cast<int>(WindowSize.y), SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_VULKAN);

#endif

	MainRenderer = SDL_CreateRenderer(MainWindow, -1, 0);
	Renderer::Init(MainRenderer);
	SceneManager::Init();
	AssetManager::Init();

	Load();
	IMGUI_CHECKVERSION();
	
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui_ImplSDL2_InitForSDLRenderer(
		MainWindow,
		MainRenderer
	);
	ImGui_ImplSDLRenderer2_Init(MainRenderer);

	
#ifdef _DEBUG
	GameViewport = &WindowManager->Insert(new Viewport(MainRenderer));
	WorldViewWindow = &WindowManager->Insert(new WorldView());
	WindowManager->Insert(new Properties(WorldViewWindow));
	WindowManager->Insert(new ContentWindow());
	WindowManager->Insert(new TextureEditor());
	WindowManager->Insert(new GameEngineSettings(this));
#endif
}

GameEngine::~GameEngine()
{
	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SceneManager::CleanUp();
	TextureManager::CleanUp();
	AssetManager::CleanUp();
	Renderer::CleanUp();
#ifdef _DEBUG
	delete WindowManager;
#endif
	SDL_DestroyRenderer(MainRenderer);
	SDL_DestroyWindow(MainWindow);
	SDL_Quit();
}

void GameEngine::Load()
{
	static std::string FILEPATH = "settings.GAMESETTINGS";
	if (!std::filesystem::exists(FILEPATH)) {
		return;
	}

	std::ifstream inputstream;
	inputstream.open(FILEPATH, std::ios_base::in | std::ios_base::binary);
	if (!inputstream.is_open()) {
		return;
	}
	
	StartScene.Load(inputstream);
}

void GameEngine::Save()
{
	static std::string FILEPATH = "settings.GAMESETTINGS";

	std::ofstream outputstream;
	outputstream.open(FILEPATH, std::ios_base::out | std::ios_base::binary);
	if(!outputstream.is_open()) {
		return;
	}

	StartScene.Save(outputstream);
}

void GameEngine::Run()
{	
	if (StartScene.Get()) {
		SceneManager::OpenScene(*StartScene.Get());
	}

	constexpr int MSPREFRAME{ 1000 / 144 };
	constexpr float FIXEDTIMESTEP{ .02f };

	auto lastTime = std::chrono::high_resolution_clock::now();
	float lag = 0.0f;

	bool isRunning = true;
	while (isRunning)
	{
		const auto currentTime = std::chrono::high_resolution_clock::now();
		const float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();

		Time::DeltaTime = deltaTime;
		lastTime = currentTime;
		lag += deltaTime;

		while (lag >= FIXEDTIMESTEP)
		{
			// fixed update
			lag -= FIXEDTIMESTEP;

		}

		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT)
			{
				isRunning = false;
			}
			ImGui_ImplSDL2_ProcessEvent(&e);
		}

		// -----------------------
		// ImGui start
		// -----------------------

#ifdef _DEBUG
		ImGui_ImplSDLRenderer2_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGuiID dockspace_id = ImGui::GetID("My Dockspace");
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		// Create settings 
		if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
		{
			ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
			ImGuiID dock_id_left = 0;
			ImGuiID dock_id_main = dockspace_id;
			ImGuiID dock_id_top = 0;
			ImGuiID dock_id_bottom = 0;
			ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, &dock_id_left, &dock_id_main);
			ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Up, 0.10f, &dock_id_top, &dock_id_bottom);
			ImGuiID dock_id_left_top = 0;
			ImGuiID dock_id_left_bottom = 0;
			ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Up, 0.50f, &dock_id_left_top, &dock_id_left_bottom);
			ImGui::DockBuilderDockWindow("Viewport", dock_id_main);
			ImGui::DockBuilderDockWindow("Properties", dock_id_left_top);
			ImGui::DockBuilderDockWindow("World View", dock_id_left_bottom);
			ImGui::DockBuilderDockWindow("Content", dock_id_bottom);
			ImGui::DockBuilderFinish(dockspace_id);
		}
#endif
		// -----------------------
		// RENDER INTO VIEWPORT
		// -----------------------
#ifdef _DEBUG
		GameViewport->BeginFrame();

		SceneManager::UpdateScene(deltaTime);
		Renderer::RenderAll();

		GameViewport->EndFrame();
#endif

		// -----------------------
		// MAIN BACKBUFFER
		// -----------------------

		SDL_SetRenderDrawColor(MainRenderer, 0, 0, 0, 255);
		SDL_RenderClear(MainRenderer);

#ifndef _DEBUG
		SceneManager::UpdateScene(deltaTime);
		Renderer::RenderAll();
#endif


#ifdef _DEBUG
		ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

		// ImGui UI
		WindowManager->Render();

		ImGui::Render();
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), MainRenderer);
#endif

		SDL_RenderPresent(MainRenderer);

		const auto sleep_time = currentTime + std::chrono::milliseconds(MSPREFRAME) - std::chrono::high_resolution_clock::now();
		std::this_thread::sleep_for(sleep_time);
	}
}

void GameEngine::SimulateGame()
{
	if (RunningGame) {
		return;
	}

	RunningGame = true;

	SceneManager::StartScene();
}

void GameEngine::StopGame()
{
	if (!RunningGame) {
		return;
	}

	RunningGame = false;

	if (StartScene.Get()) {
		SceneManager::OpenScene(*StartScene.Get());
	}
}
