#include "Editor/EditorApp.h"
#include "Core/Core.h"
#include "Core/FileSystem.h"
#include "Core/Project.h"

#include <SDL3/SDL_main.h>
#include <string>

#if FLEX_SHIPPING
	#error "FlexEditor is not meant to be built in Shipping. Only FlexCore and FlexRuntime are."
#endif

// Usage:
//   FlexEditor.exe                            -> project browser
//   FlexEditor.exe --project <file.flexproj>  -> open that project
//   FlexEditor.exe <file.flexproj>            -> same (for double-clicking a .flexproj later)
int main(int argc, char** argv)
{
	std::filesystem::path projectFile;
	for (int index{ 1 }; index < argc; ++index)
	{
		const std::string argument = argv[index];
		if (argument == "--project" && index + 1 < argc) {
			projectFile = Flex::Utf8ToPath(argv[++index]);
		}
		else if (argument.ends_with(Flex::Project::FileExtension)) {
			projectFile = Flex::Utf8ToPath(argument);
		}
	}

	Flex::EditorApp app;
	const bool bInitialised = app.Init(projectFile);
	if (bInitialised) {
		app.Run();
	}
	app.Shutdown();
	return bInitialised ? 0 : 1;
}
