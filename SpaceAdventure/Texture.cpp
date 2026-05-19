#include "Texture.h"
#include "TextureManager.h"
#include <imgui/imgui.h>
#include <SDL/SDL.h>

Texture::Texture(SDL_Texture* texture):
	sdltexture{texture}
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
	
	ImGui::Image((ImTextureID)sdltexture, size);
	Asset::ImGui_ContentHelper();
	ImGui::EndGroup();
}
