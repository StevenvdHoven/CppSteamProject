#include "Core/Game/GameModule.h"
#include "Core/Engine.h"
#include "Core/FileSystem.h"
#include "Core/Project.h"
#include "Core/Game/SystemRegistery.h"
#include "Core/World/ComponentRegistery.h"
#include <SDL3/SDL.h>
#include <Windows.h>
#include <utility>
#include <vector>

namespace Flex
{
	namespace
	{
		SDL_SharedObject* LoadedModule = nullptr;
		std::filesystem::path LoadedFile;

		std::vector<std::pair<uint32_t, GameModule::Listener>> Listeners;
		uint32_t NextListenerId = 1;

		// A copy, so a listener may add or remove listeners while it runs.
		std::vector<GameModule::Listener> CopyListeners()
		{
			std::vector<GameModule::Listener> listeners;
			for (const auto& [id, listener] : Listeners) {
				listeners.push_back(listener);
			}
			return listeners;
		}
	}

	bool GameModule::IsInLoadedModule(const void* address)
	{
		if (!address || !LoadedModule) {
			return false;
		}
		HMODULE owner = nullptr;
		const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
		return GetModuleHandleExW(flags, static_cast<LPCWSTR>(address), &owner) && owner == reinterpret_cast<HMODULE>(LoadedModule);
	}

	uint32_t GameModule::AddListener(Listener listener)
	{
		const uint32_t id = NextListenerId++;
		Listeners.emplace_back(id, std::move(listener));
		return id;
	}

	void GameModule::RemoveListener(uint32_t id)
	{
		std::erase_if(Listeners, [id](const auto& entry) { return entry.first == id; });
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
		for (const Listener& listener : CopyListeners()) {
			if (listener.AfterLoad) {
				listener.AfterLoad();
			}
		}
		return true;
	}

	void GameModule::Unload(size_t& outComponentTypes, size_t& outSystems)
	{
		outComponentTypes = 0;
		outSystems = 0;
		if (!LoadedModule) {
			return;
		}
		for (const Listener& listener : CopyListeners()) {
			if (listener.BeforeUnload) {
				listener.BeforeUnload();
			}
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
