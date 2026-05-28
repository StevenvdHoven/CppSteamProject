#ifdef _DEBUG
#include "Asset.h"
#include "AssetManager.h"
#include <imgui/imgui.h>

void Asset::ImGui_ContentHelper()
{
    ImVec2 previousSize = ImGui::GetItemRectSize();

    ImGui::PushItemWidth(previousSize.x);
    ImGui::PushID(this);

    char buffer[256]{};

    strncpy_s(buffer, Name.c_str(), sizeof(buffer) - 1);

    if (ImGui::InputText("##Name", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        Name = buffer;
        AssetManager::SaveAsset(this,false,true);
    }

    ImGui::PopID();
    ImGui::PopItemWidth();
}

void Asset::ImGui_BeginInteraction(const ImVec2& size)
{
    ImGui::PushID(this);

    if (ImGui::InvisibleButton("##asset_btn", size))
    {
        ImGui_OnOpen();
    }

    if (ImGui::BeginDragDropSource())
    {
        Asset* ptr = this;

        ImGui::SetDragDropPayload(
            "asset",
            &ptr,
            sizeof(ptr)
        );

        ImGui::Text(GetName().c_str());

        ImGui::EndDragDropSource();
    }
}
void Asset::ImGui_EndInteraction()
{
    ImGui::PopID();
}
#endif