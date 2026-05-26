#ifdef _DEBUG
#include "Properties.h"
#include "WorldView.h"


void Properties::ImGui_OnWindow()
{
	ImGui::Begin("Properties");
	if (GameObject* selected = WorldViewPtr->GetSelectGameObject()) {
		selected->ImGui_OnInpect();
	}
	ImGui::End();
}
#endif // _DEBUG