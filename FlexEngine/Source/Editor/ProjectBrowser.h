#pragma once
#include "Editor/EditorPreferences.h"
#include "Editor/FileDialog.h"
#include <filesystem>
#include <string>

namespace Flex
{
	class EditorApp;

	// Shown when the editor starts without a project (like Godot's project manager).
	// Opening or creating a project starts a new editor process for it and closes the browser.
	class ProjectBrowser
	{
	public:
		explicit ProjectBrowser(EditorApp& app);

		void Draw();
		void ShowError(const std::string& message) { ErrorMessage = message; }

	private:
		void DrawRecentProjects();
		void DrawNewProject();
		void DrawOpenProject();

		void OpenProject(const std::filesystem::path& projectFile);
		void CreateProject();

	private:
		EditorApp& App;
		EditorPreferences Preferences;

		FileDialog OpenDialog;
		FileDialog LocationDialog;

		char NewProjectName[65]{};
		std::filesystem::path NewProjectLocation;

		std::string ErrorMessage;
	};
}
