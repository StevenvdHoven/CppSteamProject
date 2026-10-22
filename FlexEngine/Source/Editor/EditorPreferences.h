#pragma once
#include <filesystem>
#include <vector>

namespace Flex
{
	// Editor settings shared by every editor process: the recent-projects list
	// and the last folder a project was created in.
	// Stored next to FlexEditor.exe ("portable"), so nothing is written to the user's
	// AppData folder. Switch GetFilePath() to SDL_GetPrefPath for a per-user location.
	class EditorPreferences
	{
	public:
		static constexpr size_t MaxRecentProjects = 16;

		// Every change reloads first and saves straight away, so several editor
		// processes don't overwrite each other's changes.
		void Load();
		void Save() const;

		void AddRecentProject(const std::filesystem::path& projectFile);
		void RemoveRecentProject(const std::filesystem::path& projectFile);
		const std::vector<std::filesystem::path>& GetRecentProjects() const { return RecentProjects; }

		void SetLastProjectLocation(const std::filesystem::path& location);
		const std::filesystem::path& GetLastProjectLocation() const { return LastProjectLocation; }

	private:
		static std::filesystem::path GetFilePath();

	private:
		std::vector<std::filesystem::path> RecentProjects;
		std::filesystem::path LastProjectLocation;
	};
}
