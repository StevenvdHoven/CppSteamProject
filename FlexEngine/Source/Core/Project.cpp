#include "Core/Project.h"
#include "Core/FileSystem.h"
#include "Core/Serialization/TextArchive.h"
#include <SDL3/SDL_log.h>
#include <fstream>

namespace Flex
{
	std::unique_ptr<Project> Project::Active;

	namespace
	{
		constexpr const char* PROJECTSECTION = "Project";
		constexpr const char* PHYSICSSECTION = "Physics";

		bool WriteGitIgnore(const std::filesystem::path& rootDirectory)
		{
			std::ofstream outputstream(rootDirectory / ".gitignore", std::ios::trunc);
			if (!outputstream.is_open()) {
				return false;
			}
			outputstream << "# FlexEngine per-machine and build output\n"
				<< "Saved/\n"
				<< "Binaries/\n"
				<< "Intermediate/\n"
				<< "Builds/\n";
			return outputstream.good();
		}
	}

	Project* Project::Create(const std::filesystem::path& parentDirectory, const std::string& name, std::string& outError)
	{
		if (!IsValidName(name, outError)) {
			return nullptr;
		}

		std::error_code errorCode;
		if (!std::filesystem::is_directory(parentDirectory, errorCode)) {
			outError = "Location does not exist: " + PathToUtf8(parentDirectory);
			return nullptr;
		}

		const std::filesystem::path rootDirectory = parentDirectory / Utf8ToPath(name);
		if (std::filesystem::exists(rootDirectory, errorCode) && !std::filesystem::is_empty(rootDirectory, errorCode)) {
			outError = "Folder already exists and is not empty: " + PathToUtf8(rootDirectory);
			return nullptr;
		}

		std::filesystem::create_directories(rootDirectory / "Content", errorCode);
		if (!errorCode) {
			std::filesystem::create_directories(rootDirectory / "Saved", errorCode);
		}
		if (errorCode) {
			outError = "Could not create project folders: " + errorCode.message();
			return nullptr;
		}

		auto project = std::unique_ptr<Project>(new Project());
		project->Name = name;
		project->EngineVersion = FLEX_VERSION_STRING;
		project->ProjectFile = rootDirectory / Utf8ToPath(name + FileExtension);

		if (!project->Save()) {
			outError = "Could not write " + PathToUtf8(project->ProjectFile);
			return nullptr;
		}
		WriteGitIgnore(rootDirectory);

		Active = std::move(project);
		return Active.get();
	}

	Project* Project::Open(const std::filesystem::path& projectFile, std::string& outError)
	{
		if (projectFile.extension() != FileExtension) {
			outError = "Not a " + std::string(FileExtension) + " file: " + PathToUtf8(projectFile);
			return nullptr;
		}

		auto project = std::unique_ptr<Project>(new Project());
		if (!project->Document.Load(projectFile, outError)) {
			return nullptr;
		}
		if (!project->Document.FindSection(PROJECTSECTION)) {
			outError = PathToUtf8(projectFile) + " has no [" + PROJECTSECTION + "] section.";
			return nullptr;
		}

		project->ProjectFile = std::filesystem::absolute(projectFile);
		project->Name = PathToUtf8(projectFile.stem());
		project->EngineVersion = "unknown";
		project->Serialize(ArchiveMode::Load);

		// Folders may be missing after a fresh clone (Saved/ is gitignored).
		std::error_code errorCode;
		std::filesystem::create_directories(project->GetContentDirectory(), errorCode);
		std::filesystem::create_directories(project->GetSavedDirectory(), errorCode);

		Active = std::move(project);
		return Active.get();
	}

	Project* Project::GetActive()
	{
		return Active.get();
	}

	void Project::CloseActive()
	{
		Active.reset();
	}

	bool Project::IsValidName(const std::string& name, std::string& outError)
	{
		if (name.empty()) {
			outError = "Project name can't be empty.";
			return false;
		}
		if (name.size() > 64) {
			outError = "Project name can be at most 64 characters.";
			return false;
		}

		auto isLetter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
		auto isDigit = [](char c) { return c >= '0' && c <= '9'; };

		if (!isLetter(name.front())) {
			outError = "Project name must start with a letter.";
			return false;
		}
		for (char c : name) {
			if (!isLetter(c) && !isDigit(c) && c != '_') {
				outError = "Project name may only contain letters, digits and '_'.";
				return false;
			}
		}
		return true;
	}

	bool Project::Save()
	{
		Serialize(ArchiveMode::Save);

		std::string error;
		if (!Document.Save(ProjectFile, error)) {
			SDL_Log("%s", error.c_str());
			return false;
		}
		return true;
	}

	void Project::Serialize(ArchiveMode mode)
	{
		TextArchive archive(Document.FindOrAddSection(PROJECTSECTION), mode);

		// Version 0 = files written before the text format (no version, unquoted values).
		// They load the same way, so no upgrade step is needed yet.
		const uint32_t version = archive.SerializeVersion(FileVersion);
		archive.Field("Name", Name);
		archive.Field("EngineVersion", EngineVersion);
		if (version >= 2) {
			archive.Field("StartScene", StartScene);
		}

		for (const std::string& warning : archive.GetWarnings()) {
			SDL_Log("%s: %s", PathToUtf8(ProjectFile).c_str(), warning.c_str());
		}

		// Older projects have no [Physics] section: they keep the defaults until saved.
		if (mode == ArchiveMode::Save || Document.FindSection(PHYSICSSECTION)) {
			TextArchive physicsArchive(Document.FindOrAddSection(PHYSICSSECTION), mode);
			Physics.Serialize(physicsArchive);
			for (const std::string& warning : physicsArchive.GetWarnings()) {
				SDL_Log("%s: %s", PathToUtf8(ProjectFile).c_str(), warning.c_str());
			}
		}
	}
}
