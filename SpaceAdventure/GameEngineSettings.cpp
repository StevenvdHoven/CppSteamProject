#ifdef _DEBUG
#include "GameEngineSettings.h"
#include "GameEngine.h"

GameEngineSettings::GameEngineSettings(GameEngine* engineRef):
	EngineRef{engineRef}
{
}

void GameEngineSettings::ImGui_OnWindow()
{
	ImGui::Begin("Game Engine Settings");

	EngineRef->StartScene.ImGui_OnDrawProperty("Start Scene");

	if (ImGui::Button("Save Settings")) {
		EngineRef->Save();
	}
	
	ImGui::End();
}
#endif
