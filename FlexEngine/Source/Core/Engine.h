#pragma once
#include "Core/Core.h"

namespace Flex
{
	// Entry point into FlexCore. Placeholder for now: it only owns SDL's
	// lifetime and proves the DLL boundary works. Systems get added step by step.
	class FLEX_API Engine
	{
	public:
		static bool Init();
		static void Shutdown();

		static bool IsInitialised();

		static const char* GetVersion();
		static const char* GetBuildConfigName();
	};
}
