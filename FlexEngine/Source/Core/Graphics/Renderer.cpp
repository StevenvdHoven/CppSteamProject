#include "Core/Graphics/Renderer.h"
#include <SDL3/SDL.h>

namespace Flex
{
	namespace
	{
		SDL_Renderer* SDLRenderer = nullptr;
		int LiveTextureCount = 0;
	}

	bool Renderer::Init(SDL_Window* window)
	{
		if (SDLRenderer) {
			return true;
		}

		SDLRenderer = SDL_CreateRenderer(window, nullptr);
		if (!SDLRenderer) {
			SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
			return false;
		}
		SDL_SetRenderVSync(SDLRenderer, 1);

#if FLEX_DEBUG_TOOLS
		SDL_Log("Renderer: %s", SDL_GetRendererName(SDLRenderer));
#endif
		return true;
	}

	void Renderer::Shutdown()
	{
		if (!SDLRenderer) {
			return;
		}

		if (LiveTextureCount > 0) {
			// SDL frees them together with the renderer; the Texture objects notice and won't double-free.
			SDL_Log("Renderer::Shutdown: %d texture(s) still alive. Release assets before shutting down the renderer.", LiveTextureCount);
		}

		SDL_DestroyRenderer(SDLRenderer);
		SDLRenderer = nullptr;
	}

	bool Renderer::IsInitialised()
	{
		return SDLRenderer != nullptr;
	}

	SDL_Renderer* Renderer::GetSDLRenderer()
	{
		return SDLRenderer;
	}

	int Renderer::GetLiveTextureCount()
	{
		return LiveTextureCount;
	}

	void Renderer::OnTextureCreated()
	{
		++LiveTextureCount;
	}

	void Renderer::OnTextureDestroyed()
	{
		--LiveTextureCount;
	}
}
