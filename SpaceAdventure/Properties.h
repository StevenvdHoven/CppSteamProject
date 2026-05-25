#pragma once
#include "EditorWindow.h"

class WorldView;

class Properties final : public EditorWindow
{
public:
	Properties(WorldView* worldView) : WorldViewPtr{ worldView } {}

	void ImGui_OnWindow() override;
private:
	WorldView* WorldViewPtr;
};

