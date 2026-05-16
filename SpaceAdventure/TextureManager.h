#pragma once
#include "Texture.h"
#include <map>
#include <string>

class TextureManager
{
public:
	static void CleanUp();
	static Texture LoadTexture(const char* texturepath);

private:
	using TextureMap = std::map<std::string, SDL_Texture*>;
	static TextureMap LoadedTextures;
};

