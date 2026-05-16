#pragma once
#include <SDL/SDL.h>
#include "Math.h"

class Texture;

class Renderer
{
public:
	Renderer();

	Renderer(const Renderer& other) = delete;
	Renderer& operator=(const Renderer& other) = delete;

	Renderer(Renderer&& other) = delete;
	Renderer& operator=(Renderer&& other) = delete;

	static void Init(SDL_Renderer* renderer);

	static void RenderTexture(const Texture& texture, const Math::Vector2& pos);
	static void RenderTexture(const Texture& texture, const Math::Vector2& pos, const Math::Vector2& size);

	static SDL_Renderer* GetCurrentRenderer() { return RenderTarget; }

private:
	static SDL_Renderer* RenderTarget;

};

