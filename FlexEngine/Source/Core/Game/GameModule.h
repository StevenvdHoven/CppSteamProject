#pragma once
#include "Core/Core.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace Flex
{
	class Project;

	// A project's game code: <ProjectName>.dll, built from the project's own C++ source with
	// the same configuration as the engine. Loading it is all it takes: its components
	// (FLEX_REGISTER_COMPONENT) and systems (FLEX_REGISTER_SYSTEM) register themselves.
	// One module per process. The editor unloads and reloads it to pick up a rebuilt module
	// (hot reload); FlexRuntime never unloads it.
	class FLEX_API GameModule
	{
	public:
		// <Project>/Binaries/<Config>/<Name>.dll, else <Name>.dll next to the running exe.
		// Empty when neither exists.
		static std::filesystem::path FindModuleFile(const Project& project);

		static bool Load(const std::filesystem::path& moduleFile, std::string& outError);

		// Before calling: every component of the module's types must be destroyed (clear the
		// scenes). Marks the module's component types stale and removes its systems, then
		// unloads the DLL. Returns how many component types and systems it owned.
		static void Unload(size_t& outComponentTypes, size_t& outSystems);

		static bool IsLoaded();
		static const std::filesystem::path& GetLoadedFile();

		// Is this address (a function, a vtable) inside the loaded module's DLL?
		static bool IsInLoadedModule(const void* address);

		// Lets other modules (engine plugins that keep the game's types, like network packets) follow
		// the game module being swapped by hot reload. BeforeUnload runs in Unload while the old module
		// is still loaded: drop everything that points into its code. AfterLoad runs once a module has
		// loaded and its types have registered. A listener added while a module is loaded isn't told
		// about it (check IsLoaded). Both may be empty.
		struct Listener
		{
			std::function<void()> BeforeUnload;
			std::function<void()> AfterLoad;
		};
		// Returns an id for RemoveListener (never 0).
		static uint32_t AddListener(Listener listener);
		static void RemoveListener(uint32_t id);
	};
}
