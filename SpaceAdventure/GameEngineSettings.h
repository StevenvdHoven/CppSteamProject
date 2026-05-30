#ifdef _DEBUG
#pragma once
#include "EditorWindow.h"

class GameEngine;

class GameEngineSettings : public EditorWindow
{
public:
	GameEngineSettings(GameEngine* engineRef);

	void ImGui_OnWindow() override;

private:
	GameEngine* EngineRef;
};
#endif