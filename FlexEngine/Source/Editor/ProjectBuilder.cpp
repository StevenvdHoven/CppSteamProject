#include "Editor/ProjectBuilder.h"
#include "Editor/BuildTools.h"
#include "Editor/OutputLog.h"
#include "Editor/ProjectScripts.h"
#include "Core/FileSystem.h"
#include "Core/Project.h"

#include <fstream>

namespace Flex
{
	namespace
	{
		std::filesystem::path EngineBinaries(const std::string& configuration)
		{
			return BuildTools::GetEngineRoot() / "Binaries" / Utf8ToPath(configuration);
		}

		bool CopyFile(const std::filesystem::path& from, const std::filesystem::path& to, OutputLog& log)
		{
			std::error_code error;
			std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, error);
			if (error) {
				log.Add(OutputLog::Level::Error, "Could not copy " + PathToUtf8(from) + ": " + error.message());
				return false;
			}
			return true;
		}

		// Adds "Builds/" to the project's .gitignore when it's missing (build output isn't committed).
		void IgnoreBuildsFolder(const Project& project)
		{
			const std::filesystem::path gitignore = project.GetRootDirectory() / ".gitignore";
			std::ifstream input(gitignore);
			if (!input.is_open()) {
				return;
			}
			std::string line;
			while (std::getline(input, line)) {
				if (line == "Builds/" || line == "Builds") {
					return;
				}
			}
			input.close();
			std::ofstream output(gitignore, std::ios::app);
			output << "Builds/\n";
		}
	}

	std::filesystem::path ProjectBuilder::GetDefaultOutputDirectory(const Project& project)
	{
		return project.GetRootDirectory() / "Builds";
	}

	bool ProjectBuilder::CheckEngineBinaries(const std::string& configuration, std::string& outError)
	{
		const std::filesystem::path binaries = EngineBinaries(configuration);
		std::error_code error;
		for (const char* file : { "FlexRuntime.exe", "FlexCore.dll", "SDL3.dll" }) {
			if (!std::filesystem::exists(binaries / file, error)) {
				outError = "The engine's " + configuration + " binaries aren't built (" + PathToUtf8(binaries / file)
					+ " is missing). Build FlexEngine.sln in " + configuration + " in Visual Studio first.";
				return false;
			}
		}
		return true;
	}

	bool ProjectBuilder::Build(const Project& project, const std::string& configuration, const std::filesystem::path& outputDirectory, OutputLog& log)
	{
		const std::string& name = project.GetName();
		log.Add(OutputLog::Level::Info, "Building " + name + " (" + configuration + ") into " + PathToUtf8(outputDirectory / Utf8ToPath(name)) + "...");

		std::string error;
		if (!CheckEngineBinaries(configuration, error)) {
			log.Add(OutputLog::Level::Error, error);
			return false;
		}
		if (!project.GetStartScene().IsValid()) {
			log.Add(OutputLog::Level::Warning, "The project has no Start Scene (Project window): the game will start empty.");
		}

		// 1. The game module in this configuration.
		std::error_code fileError;
		std::filesystem::path moduleFile;
		if (std::filesystem::exists(ProjectScripts::GetModuleProjectFile(project), fileError)) {
			if (!ProjectScripts::BuildModule(project, configuration, log)) {
				return false;
			}
			const std::filesystem::path dllName = Utf8ToPath(name + ".dll");
			for (const std::filesystem::path& candidate : { project.GetRootDirectory() / "Binaries" / Utf8ToPath(configuration) / dllName, EngineBinaries(configuration) / dllName }) {
				if (moduleFile.empty() && std::filesystem::exists(candidate, fileError)) {
					moduleFile = candidate;
				}
			}
			if (moduleFile.empty()) {
				log.Add(OutputLog::Level::Error, "The build succeeded but " + name + ".dll wasn't found.");
				return false;
			}
		} else {
			log.Add(OutputLog::Level::Info, "The project has no scripts: building without a game module.");
		}

		// 2. A clean target folder; only a previous build (with the marker) is ever replaced.
		const std::filesystem::path target = outputDirectory / Utf8ToPath(name);
		if (std::filesystem::exists(target, fileError)) {
			if (!std::filesystem::exists(target / MarkerFile, fileError) && !std::filesystem::is_empty(target, fileError)) {
				log.Add(OutputLog::Level::Error, PathToUtf8(target) + " exists and wasn't made by Build Project; choose another output folder.");
				return false;
			}
			std::filesystem::remove_all(target, fileError);
			if (fileError) {
				log.Add(OutputLog::Level::Error, "Could not clear " + PathToUtf8(target) + " (is the game still running?): " + fileError.message());
				return false;
			}
		}
		std::filesystem::create_directories(target, fileError);

		// 3. The runtime and its DLLs, the module, the project file and the content.
		const std::filesystem::path binaries = EngineBinaries(configuration);
		bool bCopied = CopyFile(binaries / "FlexRuntime.exe", target / Utf8ToPath(name + ".exe"), log)
			&& CopyFile(binaries / "FlexCore.dll", target / "FlexCore.dll", log)
			&& CopyFile(binaries / "SDL3.dll", target / "SDL3.dll", log)
			&& CopyFile(project.GetProjectFile(), target / project.GetProjectFile().filename(), log);
		if (bCopied && !moduleFile.empty()) {
			bCopied = CopyFile(moduleFile, target / moduleFile.filename(), log);
		}
		if (bCopied) {
			std::filesystem::copy(project.GetContentDirectory(), target / "Content", std::filesystem::copy_options::recursive, fileError);
			if (fileError) {
				log.Add(OutputLog::Level::Error, "Could not copy Content/: " + fileError.message());
				bCopied = false;
			}
		}
		if (!bCopied) {
			return false;
		}
		std::ofstream(target / MarkerFile) << "Built by FlexEditor (" << configuration << "). Build Project replaces this folder.\n";

		if (outputDirectory == GetDefaultOutputDirectory(project)) {
			IgnoreBuildsFolder(project);
		}

		uintmax_t bytes = 0;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(target, fileError)) {
			if (entry.is_regular_file(fileError)) {
				bytes += entry.file_size(fileError);
			}
		}
		log.Add(OutputLog::Level::Success, "Build finished: " + PathToUtf8(target / Utf8ToPath(name + ".exe")) + " ("
			+ std::to_string(bytes / (1024 * 1024)) + " MB).");
		return true;
	}
}
