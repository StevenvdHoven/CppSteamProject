#pragma once
#include "Editor/AssetDatabase.h"
#include "Editor/BuildTools.h"
#include "Editor/EditorPlugin.h"
#include "Editor/EditorPlugins.h"
#include "Editor/FileDialog.h"
#include "Core/Assets/Asset.h"
#include "Core/Serialization/TextDocument.h"
#include "Core/Uid.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct SDL_Window;
typedef unsigned int ImGuiID;

namespace Flex
{
	class AssetEditor;
	class EditorAssetSource;
	class EditorWindow;
	class ProjectBrowser;
	struct EditorContext;

	// One editor process = at most one project.
	//  - Started without a project: shows the ProjectBrowser.
	//  - Started with --project <file>: opens that project in the editor.
	// Opening or creating another project always starts a new FlexEditor process.
	// It is also the host editor plugins register their asset types, windows and menu items with.
	class EditorApp final : public EditorPluginHost
	{
	public:
		EditorApp();
		~EditorApp();

		bool Init(const std::filesystem::path& projectFile);
		void Run();
		void Shutdown();

		// Asks to save a changed scene first.
		void RequestQuit();
		SDL_Window* GetWindow() const { return Window; }

		// Starts a new FlexEditor process. An empty path opens the project browser.
		static bool LaunchEditor(const std::filesystem::path& projectFile);
		// Starts FlexRuntime on the open project with this scene.
		static bool LaunchRuntime(const std::filesystem::path& sceneFile);

		// EditorPluginHost
		EditorContext& GetContext() override { return *Context; }
		SDL_Window* GetMainWindow() override { return Window; }
		void RegisterAssetType(const std::string& typeName, const std::vector<std::string>& extensions,
			AssetDatabase::MetaInitializer initializeMeta, AssetTypeFlags flags) override;
		void RegisterAssetOpener(const std::string& typeName, std::function<void(const AssetInfo&)> open) override;
		// Focuses the editor already open for editedAssetId, or creates one with create().
		void OpenAssetEditor(Uid editedAssetId, Uid subAssetId, const std::function<std::unique_ptr<AssetEditor>()>& create) override;
		void AddPanel(std::unique_ptr<EditorWindow> panel) override;
		void AddMenuItem(const std::string& menu, const std::string& label, const std::string& shortcut,
			std::function<void()> action, std::function<bool()> isEnabled) override;
		void AddContentCreateItem(const std::string& label, std::function<void(const std::filesystem::path& folder)> create) override;
		void RegisterThumbnail(const std::string& typeName, std::function<std::shared_ptr<Texture>(const AssetInfo&)> makeThumbnail) override;
		void AddDrawCallback(std::function<void()> draw) override;

	private:
		bool CreateWindowAndImGui(const std::string& title, int width, int height, const std::filesystem::path& imguiIniFile);
		void CreateProjectWindows();
		void RegisterWindowSettingsHandler();
		void BuildDefaultLayout(ImGuiID dockspaceId);
		void DrawProjectEditor();
		void DrawMainMenuBar();
		void DrawWindowsMenu();
		// Plugin items of one menu, after a separator when bSeparate.
		void DrawPluginMenuItems(const std::string& menu, bool bSeparate);
		void RefreshAssets();

		// Double-click in the Content window: runs the opener registered for the asset's type.
		void OpenAsset(Uid assetId);

		EditorWindow* FindPanel(const std::string& title) const;

		// Scene files
		void NewScene();
		bool OpenScene(const std::filesystem::path& sceneFile);
		void SaveScene();                                   // asks for a file name the first time
		bool SaveSceneAs(const std::filesystem::path& sceneFile);
		void RunAfterSavePrompt(std::function<void()> action); // runs now, or after "Save changes?"
		void DrawSceneDialogs();
		void UpdateWindowTitle();

		// Scripts (the project's C++ game code)
		void DrawScriptsMenu();
		void DrawNewScriptDialog();
		void RecompileScripts();
		void OnScriptsBuilt(bool bSucceeded);
		void DrawBuildProjectDialog();

		// Game module and Play mode
		void LoadGameModule();
		// Hot reload: scene to text, clear, unload the old module, load the rebuilt one, scene back.
		void ReloadGameModule();
		void StartPlay();
		void StopPlay();

	private:
		SDL_Window* Window = nullptr;
		bool bIsRunning = false;

		// Only exists while no project is open.
		std::unique_ptr<ProjectBrowser> Browser;
		FileDialog OpenProjectDialog;

		AssetDatabase Assets;
		AssetRefreshReport LastAssetRefresh;
		std::unique_ptr<EditorAssetSource> AssetSourceForEditor;
		std::unique_ptr<EditorContext> Context;

		// Fixed windows (listed in the Windows menu) and the asset editors opened from Content.
		std::vector<std::unique_ptr<EditorWindow>> Panels;
		std::vector<std::unique_ptr<AssetEditor>> AssetEditors;
		std::unordered_map<std::string, std::function<void(const AssetInfo&)>> AssetOpeners; // by asset type

		// Editor plugins and the menu items they added.
		EditorPluginManager Plugins;
		struct PluginMenuItem
		{
			std::string Menu;
			std::string Label;
			std::string Shortcut;
			std::function<void()> Action;
			std::function<bool()> IsEnabled; // empty = always enabled
		};
		std::vector<PluginMenuItem> PluginMenuItems;
		std::vector<std::function<void()>> PluginDrawCallbacks;

		bool bResetLayoutRequested = false;
		int FrontTabsCountdown = 0; // after a default layout: frames until Inspector/Content come to the front

		FileDialog OpenSceneDialog;
		FileDialog SaveSceneDialog;
		std::function<void()> ActionAfterSave; // what "Save changes?" was asked for
		bool bRequestSavePrompt = false;
		bool bContinueAfterSaveAs = false;
		std::string SceneError;
		bool bRequestSceneErrorPopup = false;
		std::filesystem::path StartupScene; // last opened scene, from EditorLayout.ini
		bool bStartupSceneHandled = false;
		std::string WindowTitle;

		BackgroundJob ScriptJob;      // a running module build
		uint32_t ModuleLoadCount = 0; // each load uses a new copy in Saved/ModuleCache
		bool bRequestNewScriptPopup = false;
		char NewScriptName[64]{};
		int NewScriptTemplate = 2;
		bool bOpenNewScript = true;
		std::string NewScriptError;

		// Build Project
		BackgroundJob BuildJob;
		bool bRequestBuildPopup = false;
		int BuildConfiguration = 0;      // 0 = Development, 1 = Shipping
		char BuildOutput[512]{};
		bool bOpenBuildFolder = true;
		std::string BuildError;
		std::filesystem::path BuildTarget; // the folder being built (to open afterwards)
		FileDialog BuildFolderDialog;

		TextDocument PlaySnapshot;    // the scene as it was when Play was pressed
		Uid PlaySelection;
		uint64_t PlayRevision = 0;
		std::vector<std::shared_ptr<Asset>> PlayKeepAlive; // assets kept loaded across Stop

		// ImGui keeps a pointer to this string, so it has to outlive the ImGui context.
		std::string ImGuiIniFile;
	};
}
