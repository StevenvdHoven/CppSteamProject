#include "TextureManager.h"
#include "Renderer.h"
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

TextureManager::TextureMap TextureManager::LoadedTextures;

void TextureManager::CleanUp()
{
	for (auto entry : LoadedTextures)
	{
		SDL_DestroyTexture(entry.second);
	}
	LoadedTextures.clear();
}

Texture TextureManager::LoadTexture(const char* texturepath)
{
	std::string path = texturepath;
	if (LoadedTextures.contains(path)) {
		return Texture(LoadedTextures[path]);
	}

	SDL_Surface* surface = IMG_Load(path.c_str());
	if (!surface)
	{
		printf("IMG_Load failed: %s\n", IMG_GetError());
	}

	SDL_Texture* loadedTexture = SDL_CreateTextureFromSurface(Renderer::GetCurrentRenderer(), surface);
	LoadedTextures.emplace(path, loadedTexture);
	return Texture(loadedTexture);
}
