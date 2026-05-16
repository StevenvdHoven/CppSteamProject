#pragma once
#include "Asset.h"

struct SDL_Texture;

class Texture : public Asset
{
public:
	Texture(SDL_Texture* texture);
	SDL_Texture* Get() const { return sdltexture; }

private:
	SDL_Texture* sdltexture;
};

