#pragma once
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/imgui_impl_sdl2.h>
#include <imgui/imgui_impl_sdlrenderer2.h>

class EditorWindow
{
public:
	EditorWindow() {};

	EditorWindow(EditorWindow& other) = delete;
	EditorWindow& operator=(EditorWindow& other) = delete;

	EditorWindow(EditorWindow&& other) = delete;
	EditorWindow& operator=(EditorWindow&& other) = delete;

	virtual void ImGui_OnWindow() {}
};

