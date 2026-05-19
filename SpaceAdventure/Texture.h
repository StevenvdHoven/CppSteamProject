#pragma once
#include "Asset.h"

struct SDL_Texture;
class TextureManager;

class Texture : public Asset
{
public:
	ASSET_LOOKUP(Texture, "texture", ".png", ".jpeg")

	Texture() :
		sdltexture{nullptr}
	{

	}
	Texture(SDL_Texture* texture);

	SDL_Texture* Get() const { return sdltexture; }

	// Inherited via Asset
	void Load(std::ifstream& inputstream) override;
	void Save(std::ofstream& ouputstream) override;

private:
	SDL_Texture* sdltexture;
	friend TextureManager;
	
};

