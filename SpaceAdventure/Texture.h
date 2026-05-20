#pragma once
#include "Asset.h"
#include <SDL/SDL.h>

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

private:
	SDL_Texture* sdltexture;
	SDL_Color tint;
	friend TextureManager;
	friend TextureEditor;
};

