#include "Renderer.h"
#include "Texture.h"
#include "RenderComponent.h"
#include <iostream>
#include <thread>

Renderer* Renderer::Instance = nullptr;
SDL_Renderer* Renderer::RenderTarget = nullptr;

Renderer::Renderer()
{
	RenderThread = std::jthread(RenderLoop);
}

Renderer::~Renderer()
{
	RenderThread.join();
}

void Renderer::RenderLoop()
{
	std::lock_guard<std::mutex> lock(RenderLock);

	std::for_each(RenderComponents.begin(), RenderComponents.end(), [](const RenderComponent* renderComp)
		{
			renderComp->Render();
		});
	
}

void Renderer::Init(SDL_Renderer* renderer)
{
	Instance = new Renderer();
	RenderTarget = renderer;
}

void Renderer::Register(RenderComponent* renderComponent)
{
	Instance->RenderComponents.insert(renderComponent);
}

void Renderer::UnRegister(RenderComponent* renderComponent)
{
	Instance->RenderComponents.erase(renderComponent);
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

