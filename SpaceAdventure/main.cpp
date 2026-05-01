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
#include "SDL/SDL.h"


int main() 
{
	SteamAPI_Init();

	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_Window* window = SDL_CreateWindow("CustomGame", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1800, 980, SDL_WINDOW_SHOWN);
	SDL_Renderer* editorRenderer = SDL_CreateRenderer(window, -1, 0);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui_ImplSDL2_InitForSDLRenderer(
		window,
		editorRenderer
	);
	ImGui_ImplSDLRenderer2_Init(editorRenderer);

	SDL_Texture* gameTexture = SDL_CreateTexture(
		editorRenderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		1280,
		720
	);

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
		ImGui_ImplSDLRenderer2_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		ImGui::NewFrame();
		// Create a dockspace in main viewport, where central node is transparent.
		ImGuiID dockspace_id = ImGui::GetID("My Dockspace");
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		// Create settings
		if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
		{
			ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
			ImGuiID dock_id_left = 0;
			ImGuiID dock_id_main = dockspace_id;
			ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, &dock_id_left, &dock_id_main);
			ImGuiID dock_id_left_top = 0;
			ImGuiID dock_id_left_bottom = 0;
			ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Up, 0.50f, &dock_id_left_top, &dock_id_left_bottom);
			ImGui::DockBuilderDockWindow("Game", dock_id_main);
			ImGui::DockBuilderDockWindow("Properties", dock_id_left_top);
			ImGui::DockBuilderDockWindow("Scene", dock_id_left_bottom);
			ImGui::DockBuilderFinish(dockspace_id);
		}
#ifdef GAME
		SDL_RenderClear(editorRenderer);
#else
		SDL_RenderClear(editorRenderer);
		SDL_SetRenderTarget(editorRenderer, gameTexture);
		SDL_SetRenderDrawColor(editorRenderer, 0, 255, 0, 255);
		SDL_RenderClear(editorRenderer);
		SDL_SetRenderDrawColor(editorRenderer, 0, 0, 0, 255);
#endif

		// draw your game here

		SDL_SetRenderTarget(editorRenderer, NULL);

#ifdef GAME
#else
		SDL_SetRenderTarget(editorRenderer, NULL);
#endif

		// Submit dockspace
		ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);


		// Get the size of the child (i.e. the whole draw size of the windows).
		ImGui::Begin("Game");

		ImVec2 size = ImGui::GetContentRegionAvail();

		ImGui::GetWindowWidth();

		if ((int)size.x > 0 && (int)size.y > 0 &&
			((int)size.x != ImGui::GetWindowWidth() || (int)size.y != ImGui::GetWindowHeight()))
		{

			SDL_DestroyTexture(gameTexture);
			gameTexture = SDL_CreateTexture(
				editorRenderer,
				SDL_PIXELFORMAT_RGBA8888,
				SDL_TEXTUREACCESS_TARGET,
				(int)size.x,
				(int)size.y
			);
		}

		ImGui::Image((ImTextureID)gameTexture, size);

		ImGui::End();

		ImGui::Begin("Properties", 0);
		ImGui::Text("Hello, world");

		ImGui::End();

		ImGui::Begin("Scene", 0);
		ImGui::Text("Hello, world");

		ImGui::End();

		ImGui::Render();

		

		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),editorRenderer);

		SDL_RenderPresent(editorRenderer);
	}

	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(editorRenderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}