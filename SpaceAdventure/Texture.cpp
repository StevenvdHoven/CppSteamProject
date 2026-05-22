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
	tint{255, 255, 255, 255},
	size{}
{
	Refresh();
}

void Texture::Load(std::ifstream& inputstream)
{
	Asset::Load(inputstream);

	// Load texture based of source
	TextureManager::LoadTexture(GetSource().c_str(), this);

	inputstream.read(reinterpret_cast<char*>(&tint), sizeof(tint));
	inputstream.read(reinterpret_cast<char*>(&crop), sizeof(crop));
}

void Texture::Save(std::ofstream& outputstream)
{
	Asset::Save(outputstream);

	outputstream.write(reinterpret_cast<const char*>(&tint), sizeof(tint));
	outputstream.write(reinterpret_cast<const char*>(&crop), sizeof(crop));
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
	float factor = IMAGEWIDTH / (float)crop.w;
	ImVec2 size(
		IMAGEWIDTH,
		crop.h * factor
	);

	ImVec2 uv0(
		crop.x / (float)width,
		crop.y / (float)height
	);

	ImVec2 uv1(
		(crop.x + crop.w) / (float)width,
		(crop.y + crop.h) / (float)height
	);
	
	// Remember where to draw
	ImVec2 pos = ImGui::GetCursorScreenPos();

	// Create invisible button
	ImGui::PushID(this);
	if (ImGui::InvisibleButton("##InvisibleButton", size))
	{
		EditorWindowManager::GetChecked().GetAssetEditor<TextureEditor>().Open(this);
	}
	ImGui::PopID();

	bool hovered = ImGui::IsItemHovered();

	// Draw image on top of the button area
	ImGui::SetCursorScreenPos(pos);

	ImVec4 tint = hovered
		? ImVec4(0.7f, 0.8f, 1.0f, 1.0f) // slight blue tint
		: ImVec4(1.f, 1.f, 1.f, 1.f);

	ImGui::ImageWithBg((ImTextureID)sdltexture, size, uv0, uv1, ImVec4{255,255,255,255}, tint);
	
	Asset::ImGui_ContentHelper();
	ImGui::EndGroup();
}

void Texture::Refresh()
{
	if (!sdltexture) {
		return;
	}

	SDL_QueryTexture(sdltexture, nullptr, nullptr, &size.x, &size.y);

	crop = SDL_Rect{ 0, 0, size.x, size.y };
}
