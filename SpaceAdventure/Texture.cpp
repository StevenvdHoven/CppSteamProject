#include "Texture.h"
#include "TextureManager.h"
#include <imgui/imgui.h>
#include "EditorWindowManager.h"
#include "TextureEditor.h"

Texture::Texture():
	Texture(nullptr)
{
}

Texture::Texture(SDL_Texture* texture):
	sdltexture{texture},
	tint{255, 255, 255, 255}
{
	
}

void Texture::Load(std::ifstream& inputstream)
{
	Asset::Load(inputstream);

	// Load texture based of source
	TextureManager::LoadTexture(GetSource().c_str(), this);
}

void Texture::Save(std::ofstream& outputstream)
{
	Asset::Save(outputstream);

}

void Texture::ImportFrom(const std::string& filepath)
{
	TextureManager::LoadTexture(filepath.c_str(), this);
}

void Texture::ImGui_ContentHelper()
{
	ImGui::BeginGroup();
	int width;
	int height;
	SDL_QueryTexture(sdltexture, nullptr, nullptr, &width, &height);

	constexpr float IMAGEWIDTH = 200.f;
	float factor = IMAGEWIDTH / std::max(1.0f, static_cast<float>(width));
	ImVec2 size(IMAGEWIDTH, height * factor);
	
	// Remember where to draw
	ImVec2 pos = ImGui::GetCursorScreenPos();

	// Create invisible button
	if (ImGui::InvisibleButton(GetName().c_str(), size))
	{
		EditorWindowManager::GetChecked().GetAssetEditor<TextureEditor>().Open(this);
	}

	bool hovered = ImGui::IsItemHovered();

	// Draw image on top of the button area
	ImGui::SetCursorScreenPos(pos);

	ImVec4 tint = hovered
		? ImVec4(0.7f, 0.8f, 1.0f, 1.0f) // slight blue tint
		: ImVec4(1.f, 1.f, 1.f, 1.f);

	ImGui::Image((ImTextureID)sdltexture, size, ImVec2(0, 0), ImVec2(1, 1), tint, tint);
	
	Asset::ImGui_ContentHelper();
	ImGui::EndGroup();
}
