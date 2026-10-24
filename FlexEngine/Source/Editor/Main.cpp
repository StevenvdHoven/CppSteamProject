#include "Editor/EditorCore.h"

#include <SDL3/SDL_main.h>

// FlexEditor.exe: everything lives in FlexEditorCore.dll, so editor plugins can use it too.
int main(int argc, char** argv)
{
	return Flex::RunEditor(argc, argv);
}
