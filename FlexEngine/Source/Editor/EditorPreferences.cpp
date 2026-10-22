#include "Editor/EditorPreferences.h"
#include "Core/FileSystem.h"
#include "Core/Serialization/TextArchive.h"
#include "Core/Serialization/TextDocument.h"
#include <SDL3/SDL.h>
#include <algorithm>

namespace Flex
{
	namespace
	{
		constexpr const char* RECENTSECTION = "RecentProjects";
		constexpr const char* NEWPROJECTSECTION = "NewProject";

		bool IsSameFile(const std::filesystem::path& first, const std::filesystem::path& second)
		{
			std::error_code errorCode;
			return std::filesystem::weakly_canonical(first, errorCode) == std::filesystem::weakly_canonical(second, errorCode);
		}
	}

	void EditorPreferences::Load()
	{
		RecentProjects.clear();
		LastProjectLocation.clear();

		const std::filesystem::path filePath = GetFilePath();
		std::error_code errorCode;
		if (filePath.empty() || !std::filesystem::exists(filePath, errorCode)) {
			return;
		}

		TextDocument document;
		std::string error;
		if (!document.Load(filePath, error)) {
			SDL_Log("Could not read editor preferences: %s", error.c_str());
			return;
		}

		if (TextSection* recentSection = document.FindSection(RECENTSECTION))
		{
			TextArchive archive(*recentSection, ArchiveMode::Load);
			for (size_t index{ 0 }; index < MaxRecentProjects; ++index)
			{
				std::string value;
				archive.Field(("Project" + std::to_string(index)).c_str(), value);
				if (!value.empty()) {
					RecentProjects.emplace_back(Utf8ToPath(value).make_preferred());
				}
			}
		}

		if (TextSection* newProjectSection = document.FindSection(NEWPROJECTSECTION))
		{
			TextArchive archive(*newProjectSection, ArchiveMode::Load);
			std::string location;
			archive.Field("LastLocation", location);
			LastProjectLocation = Utf8ToPath(location).make_preferred();
		}
	}

	void EditorPreferences::Save() const
	{
		const std::filesystem::path filePath = GetFilePath();
		if (filePath.empty()) {
			return;
		}

		TextDocument document;
		{
			TextArchive archive(document.AddSection(RECENTSECTION), ArchiveMode::Save);
			for (size_t index{ 0 }; index < RecentProjects.size(); ++index)
			{
				std::string value = PathToGenericUtf8(RecentProjects[index]);
				archive.Field(("Project" + std::to_string(index)).c_str(), value);
			}
		}
		{
			TextArchive archive(document.AddSection(NEWPROJECTSECTION), ArchiveMode::Save);
			std::string location = PathToGenericUtf8(LastProjectLocation);
			archive.Field("LastLocation", location);
		}

		std::string error;
		if (!document.Save(filePath, error)) {
			SDL_Log("Could not save editor preferences: %s", error.c_str());
		}
	}

	void EditorPreferences::AddRecentProject(const std::filesystem::path& projectFile)
	{
		Load();

		std::error_code errorCode;
		const std::filesystem::path absolutePath = std::filesystem::absolute(projectFile, errorCode);

		std::erase_if(RecentProjects, [&](const std::filesystem::path& other) {
			return IsSameFile(other, absolutePath);
		});
		RecentProjects.insert(RecentProjects.begin(), absolutePath);
		if (RecentProjects.size() > MaxRecentProjects) {
			RecentProjects.resize(MaxRecentProjects);
		}

		Save();
	}

	void EditorPreferences::RemoveRecentProject(const std::filesystem::path& projectFile)
	{
		Load();
		std::erase_if(RecentProjects, [&](const std::filesystem::path& other) {
			return IsSameFile(other, projectFile);
		});
		Save();
	}

	void EditorPreferences::SetLastProjectLocation(const std::filesystem::path& location)
	{
		Load();
		LastProjectLocation = location;
		Save();
	}

	std::filesystem::path EditorPreferences::GetFilePath()
	{
		const char* basePath = SDL_GetBasePath();
		if (!basePath) {
			SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
			return {};
		}
		return Utf8ToPath(basePath) / "EditorPreferences.ini";
	}
}
