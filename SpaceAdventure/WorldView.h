#pragma once
#include "EditorWindow.h"
class WorldView final : public EditorWindow
{
public:
	WorldView() {}

	void ImGui_OnWindow() override;
};

