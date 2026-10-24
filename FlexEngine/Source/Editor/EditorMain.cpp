#include "Editor/EditorCore.h"
#include "Editor/EditorApp.h"
#include "Core/FileSystem.h"
#include "Core/Project.h"

#include <string>

namespace Flex
{
	int RunEditor(int argc, char** argv)
	{
		std::filesystem::path projectFile;
		for (int index{ 1 }; index < argc; ++index)
		{
			const std::string argument = argv[index];
			if (argument == "--project" && index + 1 < argc) {
				projectFile = Utf8ToPath(argv[++index]);
			}
			else if (argument.ends_with(Project::FileExtension)) {
				projectFile = Utf8ToPath(argument);
			}
		}

		EditorApp app;
		const bool bInitialised = app.Init(projectFile);
		if (bInitialised) {
			app.Run();
		}
		app.Shutdown();
		return bInitialised ? 0 : 1;
	}
}
