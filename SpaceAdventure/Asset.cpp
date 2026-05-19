#include "Asset.h"
#include <imgui/imgui.h>

void Asset::ImGui_ContentHelper()
{
	ImGui::Text(Name.c_str());
}
