#include "Plugins/Steam/Editor/SteamPanel.h"
#include "Plugins/Steam/Runtime/SteamNetwork.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorPlugin.h"
#include "Core/FileSystem.h"
#include "Core/Project.h"

#include <SDL3/SDL.h>

namespace Flex::SteamEditor
{
	using namespace Flex::Steam;

	// The Steam plugin's editor part: starts Steam for projects that use it (so lobbies and the overlay
	// work in the editor), keeps it running while the editor isn't playing, adds the Steam panel and
	// puts FlexSteam.dll + steam_api64.dll into builds of those projects.
	class SteamEditorPlugin final : public EditorPlugin
	{
	public:
		const char* GetName() const override { return "Steam"; }

		void OnLoad(EditorPluginHost& host) override
		{
			Context = &host.GetContext();
			Project* project = Project::GetActive();
			bUsesSteam = project && ProjectUsesSteam(*project);

			// Without Steam in the project, pressing Play must not sign anyone in.
			Session::SetAutoInit(bUsesSteam);
			Session::SetShutdownOnQuit(false); // OnUnload does it; a quit can still be cancelled ("Save changes?")
			if (bUsesSteam) {
				Session::InitFromProject();
			}

			auto panel = std::make_unique<SteamPanel>();
			Panel = panel.get();
			host.AddPanel(std::move(panel));

			// While playing, the game's SteamUpdate system pumps Steam (and gets the main-thread packets);
			// otherwise the editor does, and main-thread packets are dropped (there's no running game).
			host.AddDrawCallback([this]() {
				if (!Context->bIsPlaying) {
					Session::Update();
					Network::DispatchMainThread(nullptr);
				}
				Panel->CollectEvents();
			});

			host.AddBuildFiles([](const Project& builtProject, const std::string& configuration) {
				std::vector<BuildFile> files;
				if (!ProjectUsesSteam(const_cast<Project&>(builtProject))) {
					return files;
				}
				const char* basePath = SDL_GetBasePath();
				if (!basePath) {
					return files;
				}
				// The engine's binaries of the build's configuration: Binaries/<Config>/, next to this editor's folder.
				const std::filesystem::path binaries = Utf8ToPath(basePath).parent_path().parent_path() / Utf8ToPath(configuration);
				for (const char* name : { "FlexSteam.dll", "steam_api64.dll" }) {
					files.push_back(BuildFile{ Utf8ToPath(name), binaries / name, {} });
				}
				return files;
			});
		}

		void OnUnload() override
		{
			Session::Shutdown();
		}

	private:
		EditorContext* Context = nullptr;
		SteamPanel* Panel = nullptr;
		bool bUsesSteam = false;
	};
}

FLEX_EDITOR_PLUGIN(Flex::SteamEditor::SteamEditorPlugin)
