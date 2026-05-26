#ifdef _DEBUG
#pragma once
#include <string>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/imgui_impl_sdl2.h>
#include <imgui/imgui_impl_sdlrenderer2.h>

class EditorWindow
{
public:
	EditorWindow():
		bIsHidden{false}
	{};

	EditorWindow(EditorWindow& other) = delete;
	EditorWindow& operator=(EditorWindow& other) = delete;

	EditorWindow(EditorWindow&& other) = delete;
	EditorWindow& operator=(EditorWindow&& other) = delete;

	virtual void ImGui_OnWindow() {}
	virtual bool IsAssetEditor() const { return false; }
	virtual std::string GetAssetType() const { return "NONE"; }

	bool IsHidden() const { return bIsHidden; }
protected:
	bool bIsHidden;
};


#define ASSET_EDITOR(AssetType) \
	bool IsAssetEditor() const override { return true; } \
	std::string GetAssetType() const override { return AssetType; } 
#endif
	
	


