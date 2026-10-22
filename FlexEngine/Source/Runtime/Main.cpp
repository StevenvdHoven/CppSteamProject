#include "Core/Engine.h"
#include "Core/Graphics/Renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// The runtime is the player that will become an exported game.exe.
// For now it only opens a window; loading a project/game module comes later.
int main(int, char**)
{
	if (!Flex::Engine::Init()) {
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("FlexRuntime (" FLEX_BUILD_CONFIG_NAME ")", 1280, 720, SDL_WINDOW_RESIZABLE);
	if (!window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		Flex::Engine::Shutdown();
		return 1;
	}

	if (!Flex::Renderer::Init(window)) {
		SDL_DestroyWindow(window);
		Flex::Engine::Shutdown();
		return 1;
	}
	SDL_Renderer* renderer = Flex::Renderer::GetSDLRenderer();

	bool bIsRunning = true;
	while (bIsRunning)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT) {
				bIsRunning = false;
			}
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);
	}

	Flex::Renderer::Shutdown();
	SDL_DestroyWindow(window);
	Flex::Engine::Shutdown();
	return 0;
}
