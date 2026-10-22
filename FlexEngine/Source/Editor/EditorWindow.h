#pragma once
#include "Core/Uid.h"
#include <imgui.h>
#include <string>

namespace Flex
{
	// Base class of every dockable editor window (Hierarchy, Viewport, asset editors, ...).
	// EditorApp owns the windows, draws the open ones each frame and lists them in the
	// Windows menu. Open/closed state is saved per project in Saved/EditorLayout.ini.
	class EditorWindow
	{
	public:
		// title: shown on the tab. id: stable ImGui id ("Title###id"), so the docked position
		// survives a title change. An empty id uses the title itself.
		explicit EditorWindow(std::string title, std::string id = {})
			: Title{ std::move(title) }
			, Id{ std::move(id) }
		{
			UpdateImGuiName();
		}
		virtual ~EditorWindow() = default;

		EditorWindow(const EditorWindow&) = delete;
		EditorWindow& operator=(const EditorWindow&) = delete;

		const std::string& GetTitle() const { return Title; }
		const std::string& GetImGuiName() const { return ImGuiName; }

		bool IsOpen() const { return bIsOpen; }
		void SetOpen(bool bOpen) { bIsOpen = bOpen; }
		void Focus() { bIsOpen = true; bFocusRequested = true; }

		// Called every frame while the window is open.
		virtual void Draw() = 0;

	protected:
		// Always pair with EndWindow, whatever it returns (ImGui::Begin/End rules).
		// The tab's close button clears bIsOpen.
		bool BeginWindow(ImGuiWindowFlags flags = 0)
		{
			if (bFocusRequested) {
				ImGui::SetNextWindowFocus();
				bFocusRequested = false;
			}
			return ImGui::Begin(ImGuiName.c_str(), &bIsOpen, flags);
		}

		void EndWindow() { ImGui::End(); }

		void SetTitle(std::string title)
		{
			Title = std::move(title);
			UpdateImGuiName();
		}

	protected:
		bool bIsOpen = true;

	private:
		void UpdateImGuiName() { ImGuiName = Id.empty() ? Title : Title + "###" + Id; }

	private:
		std::string Title;
		std::string Id;
		std::string ImGuiName;
		bool bFocusRequested = false;
	};

	// A window that edits one asset. Opened by double-clicking the asset in the Content
	// window; opening an asset that already has a window focuses that window instead.
	class AssetEditor : public EditorWindow
	{
	public:
		AssetEditor(std::string title, std::string idPrefix, Uid assetId)
			: EditorWindow(std::move(title), idPrefix + "_" + assetId.ToString())
			, AssetId{ assetId }
		{
		}

		Uid GetAssetId() const { return AssetId; }

		// Opened again while already open, optionally for one of its sub-assets (e.g. a sprite).
		virtual void OnReopen(Uid subAssetId) { (void)subAssetId; }

	private:
		Uid AssetId;
	};
}
