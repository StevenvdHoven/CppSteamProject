#include "Core/Core.h"
#include <SDL3/SDL.h>
#include <Windows.h>
#include <cstdlib>

namespace Flex
{
	void AssertFailed(const char* condition, const char* message, const char* file, int line)
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Assertion failed: %s\n  %s\n  at %s(%d)", condition, message, file, line);
		if (IsDebuggerPresent()) {
			__debugbreak();
		}
		std::abort();
	}
}
