#include "ContentWindow.h"
#include "AssetManager.h"

ContentWindow::ContentWindow()
{
	RootFolder = AssetManager::GetRootFolder();
	CurrentOpenFolder = RootFolder;
}

void ContentWindow::ImGui_OnWindow()
{
	ImGui::Begin("Content");

	ImGuiID const dockspaceId = ImGui::GetID("Content_dockspace");
	ImGuiWindowClass workspaceWindowClass;
	workspaceWindowClass.ClassId = dockspaceId;
	workspaceWindowClass.DockingAllowUnclassed = false;
	if (ImGui::DockBuilderGetNode(dockspaceId))
	{
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_NoWindowMenuButton |
			ImGuiDockNodeFlags_NoCloseButton);
		ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetContentRegionAvail());

		ImGuiID leftDockId = 0;
		ImGuiID rightDockId = 0;
		ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.1f, &leftDockId, &rightDockId);

		ImGui::DockBuilderDockWindow("FolderTree", leftDockId);
		ImGui::DockBuilderDockWindow("FolderView", rightDockId);

		ImGui::DockBuilderFinish(dockspaceId);
	}

	ImGuiDockNodeFlags const dockFlags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_HiddenTabBar | ImGuiDockNodeFlags_NoCloseButton;
	ImGui::DockSpace(dockspaceId, ImGui::GetContentRegionAvail(), dockFlags, &workspaceWindowClass);

	ImGui_FolderTreeHelper(RootFolder);
	ImGui_FolderViewHelper(CurrentOpenFolder);


	ImGui::End();
}

void ContentWindow::ImGui_FolderTreeHelper(AssetFolder* folder)
{
	ImGui::Begin("FolderTree");

	const auto& childFolders = folder->GetFolders();
	for (const auto& childFolder : childFolders) {
		ImGui_FolderTree(childFolder.get());
	}

	ImGui::End();
}

void ContentWindow::ImGui_FolderViewHelper(AssetFolder* folder)
{
	ImGui::Begin("FolderView");

	ImGui::End();
}

void ContentWindow::ImGui_FolderTree(AssetFolder* folder)
{
	if (ImGui::TreeNodeEx(folder->GetName().c_str())) {
		const auto& childFolders = folder->GetFolders();
		for (const auto& childFolder : childFolders) {
			ImGui_FolderTree(childFolder.get());
		}
		ImGui::TreePop();
	}

	if (ImGui::IsItemClicked())
	{
		CurrentOpenFolder = folder;
	}
}
