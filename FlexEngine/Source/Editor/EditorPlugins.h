#pragma once
#include "Editor/EditorWindow.h"
#include <filesystem>
#include <string>
#include <vector>

namespace Flex
{
	class EditorPlugin;
	class EditorPluginHost;
	class Project;

	// Loads the editor plugins: every DLL in Binaries/<Config>/Plugins/ that exports FLEX_EDITOR_PLUGIN.
	// Which plugins a project uses is stored in its .flexproj, so the whole team gets the same set:
	//     [EditorPlugins version=1]
	//     Paint = false     ; missing = enabled
	// Plugins are never unloaded while the editor runs (their windows and callbacks live in the DLL);
	// enabling or disabling one applies after restarting the editor.
	class EditorPluginManager
	{
	public:
		struct Entry
		{
			std::string Name;           // EditorPlugin::GetName(), or the file name when it couldn't be created
			std::filesystem::path File;
			std::string Status;         // "Loaded", "Disabled for this project", or why it wasn't loaded
			bool bHasName = false;      // the plugin could be created, so its setting can be stored
			bool bIsLoaded = false;     // OnLoad ran in this editor session
			bool bIsEnabled = true;     // the project's setting
			bool bIsWarning = false;    // loaded, but Status holds a warning (e.g. older than the editor)
			EditorPlugin* Plugin = nullptr;
		};

		EditorPluginManager() = default;
		EditorPluginManager(const EditorPluginManager&) = delete;
		EditorPluginManager& operator=(const EditorPluginManager&) = delete;

		static std::filesystem::path GetPluginDirectory();

		// Loads every plugin and calls OnLoad on the enabled ones. Call once, after the editor's own
		// asset types and windows exist and before the first asset refresh.
		void LoadAll(EditorPluginHost& host, Project& project);

		// OnUnload + destroys the plugin objects (the DLLs stay loaded until the process ends).
		void UnloadAll();

		const std::vector<Entry>& GetEntries() const { return Entries; }

		// Writes the project setting (and saves the .flexproj). Applies after a restart.
		bool SetEnabled(size_t index, bool bEnabled, Project& project);

	private:
		std::vector<Entry> Entries;
	};

	// Lists the plugins with their status; a checkbox enables/disables each one for the project.
	class PluginsWindow final : public EditorWindow
	{
	public:
		explicit PluginsWindow(EditorPluginManager& plugins) : EditorWindow("Plugins"), Plugins{ plugins } {}

		void Draw() override;

	private:
		EditorPluginManager& Plugins;
		std::string Error;
	};
}
