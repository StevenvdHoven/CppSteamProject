#pragma once
#include "Core/Core.h"

// FlexEditorCore.dll holds the whole editor (and Dear ImGui), so editor plugins can link
// against it. FlexEditor.exe only calls RunEditor.
// FLEX_EDITOR_API marks what FlexEditorCore exports; plugins import it.
#if defined(FLEX_EDITOR_EXPORTS)
	#define FLEX_EDITOR_API __declspec(dllexport)
#else
	#define FLEX_EDITOR_API __declspec(dllimport)
#endif

#if FLEX_SHIPPING
	#error "The editor and its plugins are not meant to be built in Shipping. Only FlexCore and FlexRuntime are."
#endif

namespace Flex
{
	// Usage:
	//   FlexEditor.exe                            -> project browser
	//   FlexEditor.exe --project <file.flexproj>  -> open that project
	//   FlexEditor.exe <file.flexproj>            -> same (for double-clicking a .flexproj later)
	// Returns the process exit code.
	FLEX_EDITOR_API int RunEditor(int argc, char** argv);
}
