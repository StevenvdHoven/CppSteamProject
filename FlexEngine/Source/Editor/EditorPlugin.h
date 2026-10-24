#pragma once
#include "Editor/AssetDatabase.h"
#include "Editor/EditorCore.h"
#include "Editor/ProjectBuilder.h"
#include "Core/Uid.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;

namespace Flex
{
	class AssetEditor;
	class EditorWindow;
	class Project;
	class Texture;
	struct EditorContext;

	// Bumped whenever the plugin API or a struct plugins use (EditorContext, EditorWindow, ...)
	// changes in a way that needs plugins to be rebuilt. A plugin built for another version is skipped.
	inline constexpr uint32_t EditorPluginApiVersion = 3;

	// What a plugin can add to the editor. Implemented by the editor; only valid during OnLoad
	// and while the editor runs (plugins are never unloaded before the editor shuts down).
	class EditorPluginHost
	{
	public:
		virtual ~EditorPluginHost() = default;

		virtual EditorContext& GetContext() = 0;
		virtual SDL_Window* GetMainWindow() = 0; // parent for file dialogs

		// A new asset type found by extension in Content/ (see AssetDatabase::RegisterType).
		// EditorOnly types are never copied into builds.
		virtual void RegisterAssetType(const std::string& typeName, const std::vector<std::string>& extensions,
			AssetDatabase::MetaInitializer initializeMeta = {}, AssetTypeFlags flags = AssetTypeFlags::None) = 0;

		// What double-clicking an asset of this type does (usually OpenAssetEditor).
		virtual void RegisterAssetOpener(const std::string& typeName, std::function<void(const AssetInfo&)> open) = 0;

		// Focuses the editor already open for editedAssetId (OnReopen(subAssetId)), or adds the one create() makes.
		virtual void OpenAssetEditor(Uid editedAssetId, Uid subAssetId, const std::function<std::unique_ptr<AssetEditor>()>& create) = 0;

		// A dockable window listed in the Windows menu (open/closed is saved with the layout).
		virtual void AddPanel(std::unique_ptr<EditorWindow> panel) = 0;

		// An item in a main-menu menu ("File", "Edit", "Scripts", "Windows", or a new menu with that name).
		// The shortcut is only shown; the plugin handles its own keys. isEnabled may be empty (always enabled).
		virtual void AddMenuItem(const std::string& menu, const std::string& label, const std::string& shortcut,
			std::function<void()> action, std::function<bool()> isEnabled = {}) = 0;

		// An item in the Content window's right-click "Create" menu. folder = absolute path of the shown folder.
		virtual void AddContentCreateItem(const std::string& label, std::function<void(const std::filesystem::path& folder)> create) = 0;

		// Thumbnail of the asset type's Content tiles and asset slots (called once per asset until invalidated).
		virtual void RegisterThumbnail(const std::string& typeName, std::function<std::shared_ptr<Texture>(const AssetInfo&)> makeThumbnail) = 0;

		// Called every frame after the windows are drawn (for the plugin's own popups and dialogs).
		virtual void AddDrawCallback(std::function<void()> draw) = 0;

		// Extra files for File > Build Project (e.g. the plugin's runtime DLLs). Called on the main thread when
		// a build starts, with its configuration ("Development" / "Shipping"); return none to add nothing.
		virtual void AddBuildFiles(std::function<std::vector<BuildFile>(const Project& project, const std::string& configuration)> files) = 0;
	};

	// One per plugin DLL (Binaries/<Config>/Plugins/*.dll), created with FLEX_EDITOR_PLUGIN.
	class EditorPlugin
	{
	public:
		virtual ~EditorPlugin() = default;

		// Shown in the Plugins window; also the key in the project's [EditorPlugins] section.
		virtual const char* GetName() const = 0;

		// Registers everything the plugin adds. Called once a project is open, before the first asset scan.
		virtual void OnLoad(EditorPluginHost& host) = 0;

		// Called when the editor shuts down, after every window is gone and before assets are released.
		virtual void OnUnload() {}
	};
}

// In one .cpp of a plugin DLL: FLEX_EDITOR_PLUGIN(MyPlugin)
#define FLEX_EDITOR_PLUGIN(Type) \
	extern "C" __declspec(dllexport) uint32_t FlexEditorPluginApiVersion() { return ::Flex::EditorPluginApiVersion; } \
	extern "C" __declspec(dllexport) ::Flex::EditorPlugin* FlexCreateEditorPlugin() { return new Type(); }
