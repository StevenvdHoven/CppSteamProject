#ifdef _DEBUG
#pragma once
#include "EditorWindow.h"

class AssetFolder;

class ContentWindow final : public EditorWindow
{
public:
	ContentWindow();

	void ImGui_OnWindow() override;

private:
	void ImGui_FolderTreeHelper(AssetFolder* folder);
	void ImGui_FolderViewHelper(AssetFolder* folder);

	void ImGui_FolderTree(AssetFolder* folder);

private:
	AssetFolder* RootFolder;
	AssetFolder* CurrentOpenFolder;
};
#endif // _DEBUG