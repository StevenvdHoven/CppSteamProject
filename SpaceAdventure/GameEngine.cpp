#include "GameEngine.h"

GameEngine::GameEngine()
{
	SteamAPI_Init();

	SDL_Init(SDL_INIT_EVERYTHING);
	MainWindow = SDL_CreateWindow("CustomGame", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1800, 980, SDL_WINDOW_SHOWN);
	MainRenderer = SDL_CreateRenderer(MainWindow, -1, 0);
#ifdef _DEBUG
	WindowManager = new EditorWindowManager();
#endif
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui_ImplSDL2_InitForSDLRenderer(
		MainWindow,
		MainRenderer
	);
	ImGui_ImplSDLRenderer2_Init(MainRenderer);

	SceneManager::Init();
	Renderer::Init(MainRenderer);
	AssetManager::Init();
#ifdef _DEBUG
	GameViewport = &WindowManager->Insert(new Viewport(MainRenderer));
	WorldViewWindow = &WindowManager->Insert(new WorldView());
	WindowManager->Insert(new Properties(WorldViewWindow));
	WindowManager->Insert(new ContentWindow());
	WindowManager->Insert(new TextureEditor());
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

void GameEngine::Run()
{
	bool isRunning = true;
	while (isRunning)
	{
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

		SceneManager::UpdateScene(0.0f);
		Renderer::RenderAll();

		GameViewport->EndFrame();
#endif

		// -----------------------
		// MAIN BACKBUFFER
		// -----------------------

		SDL_SetRenderDrawColor(MainRenderer, 0, 0, 0, 255);
		SDL_RenderClear(MainRenderer);

#ifndef _DEBUG
		SceneManager::UpdateScene(0.0f);
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
	}
}
