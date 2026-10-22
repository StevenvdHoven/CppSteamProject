#pragma once
#include "Core/Core.h"
#include "Core/Serialization/Archive.h"
#include "Core/Serialization/TextDocument.h"
#include <filesystem>
#include <memory>
#include <string>

namespace Flex
{
	// A FlexEngine project on disk:
	//
	//     MyGame/
	//         MyGame.flexproj   project settings (FlexEngine text format, see TextDocument)
	//         Content/          assets and scenes
	//         Saved/            per-machine data: editor layout, logs (not meant for source control)
	//         .gitignore
	//
	// One process has at most one active project. Opening another project means
	// starting another editor process, so nothing ever has to be "unloaded".
	class FLEX_API Project
	{
	public:
		static constexpr const char* FileExtension = ".flexproj";
		static constexpr uint32_t FileVersion = 1;

		// Creates <parentDirectory>/<name>/ with the layout above and makes it the active project.
		static Project* Create(const std::filesystem::path& parentDirectory, const std::string& name, std::string& outError);

		// Loads a .flexproj file and makes it the active project.
		static Project* Open(const std::filesystem::path& projectFile, std::string& outError);

		static Project* GetActive();
		static void CloseActive();

		// Names become folder, file and (later) game-module names, so they're
		// limited to letters, digits and '_', starting with a letter.
		static bool IsValidName(const std::string& name, std::string& outError);

		bool Save();

		const std::string& GetName() const { return Name; }
		const std::string& GetEngineVersion() const { return EngineVersion; }

		const std::filesystem::path& GetProjectFile() const { return ProjectFile; }
		std::filesystem::path GetRootDirectory() const { return ProjectFile.parent_path(); }
		std::filesystem::path GetContentDirectory() const { return GetRootDirectory() / "Content"; }
		std::filesystem::path GetSavedDirectory() const { return GetRootDirectory() / "Saved"; }

	private:
		Project() = default;

		// Reads (Load) or writes (Save) the [Project] section.
		void Serialize(ArchiveMode mode);

	private:
		std::string Name;
		std::string EngineVersion;
		std::filesystem::path ProjectFile;

		// The whole .flexproj, so keys and sections this version doesn't know about
		// (and comments) survive a save.
		TextDocument Document;

		static std::unique_ptr<Project> Active;
	};
}
