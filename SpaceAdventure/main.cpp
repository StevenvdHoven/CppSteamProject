#pragma once
#define SDL_MAIN_HANDLED
#include <SDL/SDL.h>
#include <imgui/imgui.h>
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
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui_ImplSDL2_InitForSDLRenderer(
		window,
		renderer
	);
	ImGui_ImplSDLRenderer2_Init(renderer);

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

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::Begin("Test",0, flags);
		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		ImGui::SetWindowSize(ImVec2(200, h));
		ImGui::Text("Hello, world");

		ImGui::End();

		ImGui::Render();

		SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
		SDL_RenderClear(renderer);

		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),renderer);

		SDL_RenderPresent(renderer);
	}

	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}