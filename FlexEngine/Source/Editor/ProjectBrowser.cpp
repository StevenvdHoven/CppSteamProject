#include "Editor/ProjectBrowser.h"
#include "Editor/EditorApp.h"
#include "Core/FileSystem.h"
#include "Core/Project.h"

#include <imgui.h>

namespace Flex
{
	ProjectBrowser::ProjectBrowser(EditorApp& app) :
		App{ app }
	{
		Preferences.Load();
		NewProjectLocation = Preferences.GetLastProjectLocation();
	}

	void ProjectBrowser::Draw()
	{
		std::filesystem::path chosenPath;
		if (OpenDialog.PollResult(chosenPath)) {
			OpenProject(chosenPath);
		}
		if (LocationDialog.PollResult(chosenPath)) {
			NewProjectLocation = chosenPath;
		}

		// One window that fills the whole OS window.
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::Begin("Project Browser", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

		if (!ErrorMessage.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			ImGui::TextWrapped("%s", ErrorMessage.c_str());
			ImGui::PopStyleColor();
			ImGui::SameLine();
			if (ImGui::SmallButton("Dismiss")) {
				ErrorMessage.clear();
			}
			ImGui::Separator();
		}

		if (ImGui::BeginTable("BrowserLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Recent", ImGuiTableColumnFlags_WidthStretch, 0.6f);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch, 0.4f);

			ImGui::TableNextColumn();
			DrawRecentProjects();

			ImGui::TableNextColumn();
			DrawNewProject();
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			DrawOpenProject();

			ImGui::EndTable();
		}

		ImGui::End();
	}

	void ProjectBrowser::DrawRecentProjects()
	{
		ImGui::SeparatorText("Recent Projects");

		const auto recentProjects = Preferences.GetRecentProjects();
		if (recentProjects.empty()) {
			ImGui::TextDisabled("No recent projects yet. Create or open one on the right.");
			return;
		}

		std::filesystem::path projectToRemove;
		for (const std::filesystem::path& projectFile : recentProjects)
		{
			std::error_code errorCode;
			const bool bExists = std::filesystem::exists(projectFile, errorCode);
			const std::string pathText = PathToUtf8(projectFile);

			ImGui::PushID(pathText.c_str());
			ImGui::BeginDisabled(!bExists);
			if (ImGui::Selectable(PathToUtf8(projectFile.stem()).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				OpenProject(projectFile);
			}
			ImGui::EndDisabled();

			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Open", nullptr, false, bExists)) {
					OpenProject(projectFile);
				}
				if (ImGui::MenuItem("Remove from list")) {
					projectToRemove = projectFile;
				}
				ImGui::EndPopup();
			}

			ImGui::Indent();
			ImGui::TextDisabled("%s%s", pathText.c_str(), bExists ? "" : "  (missing)");
			ImGui::Unindent();
			ImGui::PopID();
		}

		if (!projectToRemove.empty()) {
			Preferences.RemoveRecentProject(projectToRemove);
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Double-click to open. Right-click for more.");
	}

	void ProjectBrowser::DrawNewProject()
	{
		ImGui::SeparatorText("New Project");

		ImGui::InputText("Name", NewProjectName, sizeof(NewProjectName));

		const std::string locationText = NewProjectLocation.empty() ? "(choose a folder)" : PathToUtf8(NewProjectLocation);
		ImGui::TextWrapped("Location: %s", locationText.c_str());
		ImGui::BeginDisabled(LocationDialog.IsOpen());
		if (ImGui::Button("Browse...")) {
			LocationDialog.OpenFolder(App.GetWindow(), NewProjectLocation);
		}
		ImGui::EndDisabled();

		std::string nameError;
		const bool bValidName = Project::IsValidName(NewProjectName, nameError);
		const bool bCanCreate = bValidName && !NewProjectLocation.empty();

		if (NewProjectName[0] != '\0' && !bValidName) {
			ImGui::TextDisabled("%s", nameError.c_str());
		}
		else if (bCanCreate) {
			const std::string targetText = PathToUtf8(NewProjectLocation / Utf8ToPath(NewProjectName));
			ImGui::TextDisabled("Creates %s", targetText.c_str());
		}

		ImGui::BeginDisabled(!bCanCreate);
		if (ImGui::Button("Create & Open")) {
			CreateProject();
		}
		ImGui::EndDisabled();
	}

	void ProjectBrowser::DrawOpenProject()
	{
		ImGui::SeparatorText("Open Project");
		ImGui::BeginDisabled(OpenDialog.IsOpen());
		if (ImGui::Button("Open .flexproj...")) {
			OpenDialog.OpenFile(App.GetWindow(), "FlexEngine Project", "flexproj", Preferences.GetLastProjectLocation());
		}
		ImGui::EndDisabled();
	}

	void ProjectBrowser::OpenProject(const std::filesystem::path& projectFile)
	{
		std::error_code errorCode;
		if (!std::filesystem::exists(projectFile, errorCode)) {
			ErrorMessage = "Project file not found: " + PathToUtf8(projectFile);
			return;
		}

		if (!EditorApp::LaunchEditor(projectFile)) {
			ErrorMessage = "Could not start a new editor for " + PathToUtf8(projectFile);
			return;
		}
		App.RequestQuit();
	}

	void ProjectBrowser::CreateProject()
	{
		std::string error;
		Project* project = Project::Create(NewProjectLocation, NewProjectName, error);
		if (!project) {
			ErrorMessage = error;
			return;
		}

		const std::filesystem::path projectFile = project->GetProjectFile();
		Project::CloseActive(); // the new editor process opens it

		Preferences.SetLastProjectLocation(NewProjectLocation);
		OpenProject(projectFile);
	}
}
