#include "Core/Engine.h"
#include <SDL3/SDL.h>

namespace Flex
{
	namespace
	{
		bool bInitialised = false;
	}

	bool Engine::Init()
	{
		if (bInitialised) {
			return true;
		}

		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
			SDL_Log("SDL_Init failed: %s", SDL_GetError());
			return false;
		}

#if FLEX_DEBUG_TOOLS
		const int sdlVersion = SDL_GetVersion();
		SDL_Log("FlexEngine %s (%s) - SDL %d.%d.%d",
			GetVersion(),
			GetBuildConfigName(),
			SDL_VERSIONNUM_MAJOR(sdlVersion),
			SDL_VERSIONNUM_MINOR(sdlVersion),
			SDL_VERSIONNUM_MICRO(sdlVersion));
#endif

		bInitialised = true;
		return true;
	}

	void Engine::Shutdown()
	{
		if (!bInitialised) {
			return;
		}

		SDL_Quit();
		bInitialised = false;
	}

	bool Engine::IsInitialised()
	{
		return bInitialised;
	}

	const char* Engine::GetVersion()
	{
		return FLEX_VERSION_STRING;
	}

	const char* Engine::GetBuildConfigName()
	{
		return FLEX_BUILD_CONFIG_NAME;
	}
}
