#pragma once
#include "Core/Core.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace Flex
{
	// Owns the one SDL_Renderer of the process. Everything that creates GPU
	// resources (textures) goes through here, so the backend can be swapped later
	// (OpenGL / SDL_GPU batch renderer) without touching assets or game code.
	//
	// Shutdown order matters: release all textures (AssetManager::Shutdown) BEFORE Renderer::Shutdown.
	class FLEX_API Renderer
	{
	public:
		static bool Init(SDL_Window* window);
		static void Shutdown();

		static bool IsInitialised();

		// Escape hatch for code that talks to SDL directly (ImGui backend, editor previews).
		static SDL_Renderer* GetSDLRenderer();

		// Number of GPU textures currently alive (debug/statistics).
		static int GetLiveTextureCount();

		// Call after SDL_RenderPresent: frees the GPU textures replaced during the frame
		// (Texture::UpdatePixels with a new size), which this frame's draw commands could still use.
		static void EndFrame();

	private:
		friend class Texture;
		static void OnTextureCreated();
		static void OnTextureDestroyed();
		static void DestroyAfterFrame(SDL_Texture* handle);
	};
}
