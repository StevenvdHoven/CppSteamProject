#pragma once
#include "Asset.h"
#include <SDL/SDL.h>
#include <imgui/imgui_internal.h>

class TextureManager;
class TextureEditor;

class Texture : public Asset
{
public:
	ASSET_LOOKUP(Texture, "texture", ".png", ".jpeg")

	Texture();
	Texture(SDL_Texture* texture);

	SDL_Texture* Get() const { return sdltexture; }

	// Inherited via Asset
	void Load(std::ifstream& inputstream) override;
	void Save(std::ofstream& ouputstream) override;
	void ImportFrom(const std::string& filepath) override;

	void ImGui_ContentHelper() override;

	void Refresh();

private:
	SDL_Texture* sdltexture;
	SDL_Color tint;
	SDL_Rect crop;
	ImVec2i size;
	friend TextureManager;
	friend TextureEditor;
};

