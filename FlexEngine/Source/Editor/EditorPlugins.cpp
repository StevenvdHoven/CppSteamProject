#include "Editor/EditorPlugins.h"
#include "Editor/EditorPlugin.h"
#include "Core/FileSystem.h"
#include "Core/Project.h"
#include "Core/Serialization/TextDocument.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <algorithm>

namespace Flex
{
	namespace
	{
		constexpr const char* PLUGINSECTION = "EditorPlugins";
		constexpr const char* PLUGINSECTIONVERSION = "1";

		using ApiVersionFunction = uint32_t (*)();
		using CreatePluginFunction = EditorPlugin* (*)();

		bool IsEnabledInProject(Project& project, const std::string& name)
		{
			const TextSection* section = project.GetDocument().FindSection(PLUGINSECTION);
			const std::string* value = section ? section->GetValue(name) : nullptr;
			return !value || *value != "false";
		}
	}

	std::filesystem::path EditorPluginManager::GetPluginDirectory()
	{
		// SDL_GetBasePath is the folder of the running exe (Binaries/<Config>/), with a trailing separator.
		const char* basePath = SDL_GetBasePath();
		return basePath ? Utf8ToPath(basePath) / "Plugins" : std::filesystem::path("Plugins");
	}

	void EditorPluginManager::LoadAll(EditorPluginHost& host, Project& project)
	{
		const std::filesystem::path directory = GetPluginDirectory();
		std::error_code error;
		std::vector<std::filesystem::path> files;
		for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
			if (entry.is_regular_file(error) && entry.path().extension() == ".dll") {
				files.push_back(entry.path());
			}
		}
		std::sort(files.begin(), files.end());

		// A plugin older than the editor may expect other struct layouts: warn (the API version catches deliberate changes).
		const char* basePath = SDL_GetBasePath();
		const std::filesystem::path editorCore = basePath ? Utf8ToPath(basePath) / "FlexEditorCore.dll" : std::filesystem::path();
		const auto editorTime = std::filesystem::last_write_time(editorCore, error);
		const bool bHasEditorTime = !error;

		for (const std::filesystem::path& file : files) {
			Entry entry;
			entry.Name = PathToUtf8(file.stem());
			entry.File = file;

			SDL_SharedObject* library = SDL_LoadObject(PathToUtf8(file).c_str());
			if (!library) {
				entry.Status = std::string("Could not load: ") + SDL_GetError();
				SDL_Log("Editor plugin %s: %s", entry.Name.c_str(), entry.Status.c_str());
				Entries.push_back(std::move(entry));
				continue;
			}

			const auto apiVersion = reinterpret_cast<ApiVersionFunction>(SDL_LoadFunction(library, "FlexEditorPluginApiVersion"));
			const auto createPlugin = reinterpret_cast<CreatePluginFunction>(SDL_LoadFunction(library, "FlexCreateEditorPlugin"));
			if (!apiVersion || !createPlugin) {
				entry.Status = "Not an editor plugin (no FLEX_EDITOR_PLUGIN).";
			} else if (apiVersion() != EditorPluginApiVersion) {
				entry.Status = "Built for plugin API " + std::to_string(apiVersion()) + ", the editor has "
					+ std::to_string(EditorPluginApiVersion) + ": rebuild the plugin.";
			} else {
				entry.Plugin = createPlugin();
			}
			if (!entry.Plugin) {
				SDL_Log("Editor plugin %s: %s", entry.Name.c_str(), entry.Status.c_str());
				SDL_UnloadObject(library);
				Entries.push_back(std::move(entry));
				continue;
			}

			entry.Name = entry.Plugin->GetName();
			const bool bDuplicate = std::any_of(Entries.begin(), Entries.end(), [&entry](const Entry& other) { return other.Plugin && other.Name == entry.Name; });
			entry.bIsEnabled = IsEnabledInProject(project, entry.Name);
			entry.bHasName = !bDuplicate;
			if (bDuplicate || !entry.bIsEnabled) {
				entry.Status = bDuplicate ? "Another plugin has the same name." : "Disabled for this project.";
				delete entry.Plugin;
				entry.Plugin = nullptr;
				SDL_UnloadObject(library);
				Entries.push_back(std::move(entry));
				continue;
			}

			// Deliberately never unloaded: the plugin's windows and callbacks live in the DLL.
			entry.Plugin->OnLoad(host);
			entry.bIsLoaded = true;
			entry.Status = "Loaded";
			const auto pluginTime = std::filesystem::last_write_time(file, error);
			if (bHasEditorTime && !error && pluginTime < editorTime) {
				entry.Status = "Loaded, but older than FlexEditorCore.dll: rebuild it if the editor changed.";
				entry.bIsWarning = true;
			}
			SDL_Log("Editor plugin %s: %s", entry.Name.c_str(), entry.Status.c_str());
			Entries.push_back(std::move(entry));
		}
	}

	void EditorPluginManager::UnloadAll()
	{
		for (Entry& entry : Entries) {
			if (entry.Plugin) {
				entry.Plugin->OnUnload();
				delete entry.Plugin;
				entry.Plugin = nullptr;
			}
		}
	}

	bool EditorPluginManager::SetEnabled(size_t index, bool bEnabled, Project& project)
	{
		Entry& entry = Entries[index];
		TextSection& section = project.GetDocument().FindOrAddSection(PLUGINSECTION);
		section.SetAttribute("version", PLUGINSECTIONVERSION);
		section.SetValue(entry.Name, bEnabled ? "true" : "false");
		entry.bIsEnabled = bEnabled;
		return project.Save();
	}

	void PluginsWindow::Draw()
	{
		if (!BeginWindow()) {
			EndWindow();
			return;
		}

		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
		ImGui::TextWrapped("Plugins are DLLs in %s. Enabling or disabling one applies after restarting the editor.",
			PathToUtf8(EditorPluginManager::GetPluginDirectory()).c_str());
		ImGui::PopStyleColor();
		if (!Error.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", Error.c_str());
		}

		const std::vector<EditorPluginManager::Entry>& entries = Plugins.GetEntries();
		if (entries.empty()) {
			ImGui::TextUnformatted("No plugins found.");
		} else if (ImGui::BeginTable("Plugins", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_WidthFixed, 140.0f);
			ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();
			for (size_t index = 0; index < entries.size(); ++index) {
				const EditorPluginManager::Entry& entry = entries[index];
				ImGui::PushID(static_cast<int>(index));
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				const bool bCanToggle = entry.bHasName;
				bool bEnabled = entry.bIsEnabled;
				ImGui::BeginDisabled(!bCanToggle);
				if (ImGui::Checkbox("##Enabled", &bEnabled)) {
					Project* project = Project::GetActive();
					Error = project && Plugins.SetEnabled(index, bEnabled, *project) ? std::string() : "Could not save the project file.";
				}
				ImGui::EndDisabled();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(entry.Name.c_str());
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("%s", PathToUtf8(entry.File).c_str());
				}
				ImGui::TableNextColumn();
				const bool bNeedsRestart = bCanToggle && entry.bIsEnabled != entry.bIsLoaded;
				if (bNeedsRestart) {
					ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Restart the editor to %s it.", entry.bIsEnabled ? "load" : "unload");
				} else if (entry.bIsLoaded && !entry.bIsWarning) {
					ImGui::TextUnformatted(entry.Status.c_str());
				} else if (entry.bHasName) {
					ImGui::TextColored(entry.bIsWarning ? ImVec4(1.0f, 0.85f, 0.4f, 1.0f) : ImGui::GetStyle().Colors[ImGuiCol_TextDisabled], "%s", entry.Status.c_str());
				} else {
					ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", entry.Status.c_str());
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		EndWindow();
	}
}
