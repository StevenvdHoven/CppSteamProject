#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace Flex
{
	class OutputLog;
	class Project;

	// File > Build Project: a standalone folder that runs the game without the editor.
	//
	//     <Output>/<Name>/
	//         <Name>.exe          (FlexRuntime of that configuration, renamed)
	//         FlexCore.dll, SDL3.dll
	//         <Name>.dll          (the game module, built in that configuration)
	//         <Name>.flexproj     (FlexRuntime finds it next to itself and runs its Start Scene)
	//         Content/            (as it is, with .meta files; cooked data comes later)
	//         ...                 (files editor plugins add, see BuildFile)
	//         .flexbuild          (marker: only folders with it are ever replaced)

	// A file an editor plugin adds to a build (EditorPluginHost::AddBuildFiles), e.g. its runtime DLLs.
	struct BuildFile
	{
		std::filesystem::path Target; // relative to the build folder, e.g. "steam_api64.dll"
		std::filesystem::path Source; // the file to copy; empty = write Text instead
		std::string Text;
	};

	namespace ProjectBuilder
	{
		constexpr const char* MarkerFile = ".flexbuild";

		// <Project>/Builds
		std::filesystem::path GetDefaultOutputDirectory(const Project& project);

		// False when the engine's binaries of that configuration haven't been built.
		bool CheckEngineBinaries(const std::string& configuration, std::string& outError);

		// Blocking (run it on a background thread). Progress and problems go to log.
		// skippedExtensions: lower-case extensions of editor-only assets (AssetDatabase::GetEditorOnlyExtensions),
		// left out of the copied Content/ together with their .meta files. extraFiles: from editor plugins.
		bool Build(const Project& project, const std::string& configuration, const std::filesystem::path& outputDirectory,
			const std::vector<std::string>& skippedExtensions, const std::vector<BuildFile>& extraFiles, OutputLog& log);
	}
}
