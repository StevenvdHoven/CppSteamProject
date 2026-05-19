#include "Texture.h"
#include "TextureManager.h"

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
