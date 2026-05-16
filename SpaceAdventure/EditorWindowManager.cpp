#include "EditorWindowManager.h"

void EditorWindowManager::Render()
{
	for (auto& editorWindow : EditorWindows) {
		editorWindow->ImGui_OnWindow();
	}
}
