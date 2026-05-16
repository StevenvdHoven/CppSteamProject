#include "Renderer.h"
#include "Texture.h"

SDL_Renderer* Renderer::RenderTarget = nullptr;

void Renderer::Init(SDL_Renderer* renderer)
{
	RenderTarget = renderer;
}

void Renderer::RenderTexture(const Texture& texture, const Math::Vector2& pos)
{ 
	SDL_Rect dsrect{};
	dsrect.x = pos.x;
	dsrect.y = pos.y;
	SDL_QueryTexture(texture.Get(), nullptr, nullptr, &dsrect.w, &dsrect.h);
	SDL_RenderCopy(RenderTarget, texture.Get(), nullptr, &dsrect);
}

void Renderer::RenderTexture(const Texture& texture, const Math::Vector2& pos, const Math::Vector2& size)
{
	SDL_Rect dsrect{};
	dsrect.x = pos.x;
	dsrect.y = pos.y;
	dsrect.w = size.x;	
	dsrect.h = size.y;
	SDL_RenderCopy(RenderTarget, texture.Get(), nullptr, &dsrect);
}

