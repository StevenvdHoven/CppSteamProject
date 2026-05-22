#include "Asset.h"
#include "AssetManager.h"
#include <imgui/imgui.h>

void Asset::ImGui_ContentHelper()
{
    ImVec2 previousSize = ImGui::GetItemRectSize();

    ImGui::PushItemWidth(previousSize.x);
    ImGui::PushID(this);

    char buffer[256]{};

    strncpy(buffer, Name.c_str(), sizeof(buffer) - 1);

    if (ImGui::InputText("##Name", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        Name = buffer;
        AssetManager::SaveAsset(this,false,true);
    }

    ImGui::PopID();
    ImGui::PopItemWidth();
}
