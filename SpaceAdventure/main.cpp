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
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include "Renderer.h"
#include "AssetManager.h"
#include "EditorWindowManager.h"
#include "ContentWindow.h"
#include "Viewport.h"
#include "WorldView.h"
#include "Properties.h"
#include "TextureManager.h"
#include "TextureEditor.h"


int main() 
{
	SteamAPI_Init();

	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_Window* window = SDL_CreateWindow("CustomGame", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1800, 980, SDL_WINDOW_SHOWN);
	SDL_Renderer* editorRenderer = SDL_CreateRenderer(window, -1, 0);
#ifdef _DEBUG
	EditorWindowManager* windowManager = new EditorWindowManager();
#endif
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui_ImplSDL2_InitForSDLRenderer(
		window,
		editorRenderer
	);
	ImGui_ImplSDLRenderer2_Init(editorRenderer);

	Renderer::Init(editorRenderer);
	AssetManager::Init();
#ifdef _DEBUG
	Viewport& gameviewport = windowManager->Insert(new Viewport(editorRenderer));
	windowManager->Insert(new WorldView());
	windowManager->Insert(new Properties());
	windowManager->Insert(new ContentWindow());
	windowManager->Insert(new TextureEditor());
#endif

	Texture testTexture = TextureManager::LoadTexture("character2.png");
	Texture copiedTexture = TextureManager::LoadTexture("character2.png");

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
		gameviewport.BeginFrame();

		Renderer::RenderTexture(testTexture, { 0, 0 });

		Renderer::RenderTexture(copiedTexture, { 128, 0 });
		gameviewport.EndFrame();
#endif

		// -----------------------
		// MAIN BACKBUFFER
		// -----------------------

		SDL_SetRenderDrawColor(editorRenderer, 0, 0, 0, 255);
		SDL_RenderClear(editorRenderer);

#ifdef _DEBUG
		ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

		// ImGui UI
		windowManager->Render();

		ImGui::Render();
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), editorRenderer);
#endif

		SDL_RenderPresent(editorRenderer);
	}

	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	TextureManager::CleanUp();
	AssetManager::CleanUp();

	SDL_DestroyRenderer(editorRenderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}