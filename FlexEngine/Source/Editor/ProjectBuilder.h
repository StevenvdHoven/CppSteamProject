#pragma once
#include <filesystem>
#include <string>

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
	//         .flexbuild          (marker: only folders with it are ever replaced)
	namespace ProjectBuilder
	{
		constexpr const char* MarkerFile = ".flexbuild";

		// <Project>/Builds
		std::filesystem::path GetDefaultOutputDirectory(const Project& project);

		// False when the engine's binaries of that configuration haven't been built.
		bool CheckEngineBinaries(const std::string& configuration, std::string& outError);

		// Blocking (run it on a background thread). Progress and problems go to log.
		bool Build(const Project& project, const std::string& configuration, const std::filesystem::path& outputDirectory, OutputLog& log);
	}
}
