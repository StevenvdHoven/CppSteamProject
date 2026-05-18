#pragma once
#include "Asset.h"

struct SDL_Texture;

class Texture : public Asset
{
public:
	Texture(SDL_Texture* texture);

	SDL_Texture* Get() const { return sdltexture; }

	// Inherited via Asset
	void Load(std::ifstream& inputstream) override;
	void Save(std::ofstream& ouputstream) override;

private:
	SDL_Texture* sdltexture;

	
};

