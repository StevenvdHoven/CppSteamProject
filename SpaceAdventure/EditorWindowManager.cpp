#ifdef _DEBUG
#include "EditorWindowManager.h"

EditorWindowManager* EditorWindowManager::Instance = nullptr;

void EditorWindowManager::Render()
{
	for (auto& editorWindow : EditorWindows) {
		if (editorWindow->IsHidden()) {
			continue;
		}
		editorWindow->ImGui_OnWindow();
	}
}
#endif