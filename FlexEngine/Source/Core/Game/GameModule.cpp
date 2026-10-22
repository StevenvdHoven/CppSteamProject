#include "Core/Game/GameModule.h"
#include "Core/Engine.h"
#include "Core/FileSystem.h"
#include "Core/Project.h"
#include "Core/Game/SystemRegistery.h"
#include "Core/World/ComponentRegistery.h"
#include <SDL3/SDL.h>
#include <Windows.h>

namespace Flex
{
	namespace
	{
		SDL_SharedObject* LoadedModule = nullptr;
		std::filesystem::path LoadedFile;

		// Is this function inside the loaded module's DLL?
		bool IsInLoadedModule(const void* function)
		{
			if (!function || !LoadedModule) {
				return false;
			}
			HMODULE owner = nullptr;
			const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
			return GetModuleHandleExW(flags, static_cast<LPCWSTR>(function), &owner) && owner == reinterpret_cast<HMODULE>(LoadedModule);
		}
	}

	std::filesystem::path GameModule::FindModuleFile(const Project& project)
	{
		const std::filesystem::path fileName = Utf8ToPath(project.GetName() + ".dll");
		std::error_code error;

		const std::filesystem::path inProject = project.GetRootDirectory() / "Binaries" / Engine::GetBuildConfigName() / fileName;
		if (std::filesystem::exists(inProject, error)) {
			return inProject;
		}
		if (const char* basePath = SDL_GetBasePath()) {
			const std::filesystem::path nextToExe = Utf8ToPath(basePath) / fileName;
			if (std::filesystem::exists(nextToExe, error)) {
				return nextToExe;
			}
		}
		return {};
	}

	bool GameModule::Load(const std::filesystem::path& moduleFile, std::string& outError)
	{
		if (LoadedModule) {
			outError = "A game module is already loaded (" + PathToUtf8(LoadedFile) + ").";
			return false;
		}
		LoadedModule = SDL_LoadObject(PathToUtf8(moduleFile).c_str());
		if (!LoadedModule) {
			outError = "Could not load " + PathToUtf8(moduleFile) + ": " + SDL_GetError();
			return false;
		}
		LoadedFile = moduleFile;
		SDL_Log("Game module loaded: %s", PathToUtf8(moduleFile).c_str());
		return true;
	}

	void GameModule::Unload(size_t& outComponentTypes, size_t& outSystems)
	{
		outComponentTypes = 0;
		outSystems = 0;
		if (!LoadedModule) {
			return;
		}
		// A type belongs to the module when any of its functions lives in the module's code.
		outComponentTypes = ComponentRegistery::MarkStale([](const ComponentTypeInfo& type) {
			return IsInLoadedModule(reinterpret_cast<const void*>(type.Relocate)) || IsInLoadedModule(reinterpret_cast<const void*>(type.Destruct))
				|| IsInLoadedModule(reinterpret_cast<const void*>(type.DefaultConstruct)) || IsInLoadedModule(reinterpret_cast<const void*>(type.Serialize));
		});
		outSystems = SystemRegistery::Remove([](const SystemInfo& system) {
			return IsInLoadedModule(reinterpret_cast<const void*>(system.Function));
		});
		SDL_UnloadObject(LoadedModule);
		LoadedModule = nullptr;
		SDL_Log("Game module unloaded: %s", PathToUtf8(LoadedFile).c_str());
		LoadedFile.clear();
	}

	bool GameModule::IsLoaded()
	{
		return LoadedModule != nullptr;
	}

	const std::filesystem::path& GameModule::GetLoadedFile()
	{
		return LoadedFile;
	}
}
