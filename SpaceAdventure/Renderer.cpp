#include "Renderer.h"
#include "Texture.h"
#include "EditorWindowManager.h"
#include "Viewport.h"
#include "RenderComponent.h"
#include "Transform.h"
#include <iostream>
#include <thread>
#include <chrono>

Renderer* Renderer::Instance = nullptr;
SDL_Renderer* Renderer::RenderTarget = nullptr;
std::mutex Renderer::RenderLock;

Renderer::Renderer()
{
	//RenderThread = std::jthread(&Renderer::RenderLoop, this);
}

Renderer::~Renderer()
{
	RenderThread.request_stop();
	RenderThread.join();
}

void Renderer::RenderLoop()
{
	while (Instance != nullptr)
	{
		if (!RenderComponents.size()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(0));
			continue;
		}

		std::lock_guard<std::mutex> lock(RenderLock);

		std::for_each(RenderComponents.begin(), RenderComponents.end(), [](RenderComponent* renderComp)
			{
				renderComp->Render();
			});
	}
}

void Renderer::Render()
{
	std::lock_guard<std::mutex> lock(RenderLock);

	for (auto renderComp : RenderComponents) {
		renderComp->Render();
	}
}

void Renderer::Init(SDL_Renderer* renderer)
{
	Instance = new Renderer();
	RenderTarget = renderer;
}

void Renderer::CleanUp()
{
	if (Instance) {
		Renderer* temp = Instance;
		Instance = nullptr;
		delete temp;
	}
}

void Renderer::RenderAll()
{
	if (Instance)
	{
		Instance->Render();
	}
}

void Renderer::Register(RenderComponent* renderComponent)
{
	std::lock_guard<std::mutex> lock(RenderLock);
	Instance->RenderComponents.insert(renderComponent);
}

void Renderer::UnRegister(RenderComponent* renderComponent)
{
	std::lock_guard<std::mutex> lock(RenderLock);
	Instance->RenderComponents.erase(renderComponent);
}

void Renderer::RenderTexture(const Texture& texture, const Math::Vector2& pos)
{ 
	SDL_Rect srect = texture.GetCropRect();
	SDL_Rect dsrect{};
	dsrect.x = static_cast<int>(pos.x);
	dsrect.y = static_cast<int>(pos.y);
	dsrect.w = static_cast<int>(srect.w);
	dsrect.h = static_cast<int>(srect.h);
	SDL_RenderCopy(RenderTarget, texture.Get(), &srect, &dsrect);
}

void Renderer::RenderTexture(const Texture& texture, const Math::Vector2& pos, const Math::Vector2& size)
{
	SDL_Rect srect = texture.GetCropRect();
	SDL_Rect dsrect{};
	dsrect.x = static_cast<int>(pos.x);
	dsrect.y = static_cast<int>(pos.y);
	dsrect.w = static_cast<int>(srect.w);
	dsrect.h = static_cast<int>(srect.h);
	SDL_RenderCopy(RenderTarget, texture.Get(), &srect, &dsrect);
}

void Renderer::RenderTexture(const Texture& texture, Transform* transform)
{
	Math::Vector2 pos;
	float rot;
	float scale;

#ifndef _DEBUG
	ApplyCamera(transform, pos, rot, scale)
#else
	ApplyViewport(transform, pos, rot, scale);
#endif

	SDL_Rect src = texture.GetCropRect();

	SDL_Rect dst;
	dst.w = static_cast<int>(src.w * scale);
	dst.h = static_cast<int>(src.h * scale);

	dst.x = static_cast<int>(pos.x - dst.w * 0.5f);
	dst.y = static_cast<int>(pos.y - dst.h * 0.5f);

	SDL_Point center;
	center.x = dst.w / 2;
	center.y = dst.h / 2;

	SDL_RenderCopyEx(RenderTarget, texture.Get(), &src, &dst, rot, &center, SDL_FLIP_NONE);
}

void Renderer::ApplyCamera(Transform* transform, Math::Vector2& pos, float& rot, float& scale)
{
}

#ifdef _DEBUG
void Renderer::ApplyViewport(Transform* transform, Math::Vector2& pos, float& rot, float& scale)
{
	Viewport& viewport = EditorWindowManager::GetChecked().GetEditorWindow<Viewport>();
	Math::Matrix2x3 viewportMatrix = viewport.GetViewportMatrix();

	pos = viewportMatrix.Transform(transform->GetWorldLocation());
	rot = transform->GetWorldRotation();
	scale = transform->GetWorldScale() * viewportMatrix.ExtractScale();
}
#endif

