#pragma once
#include <SDL/SDL.h>
#include <vector>
#include <mutex>
#include "Math.h"

class Texture;
class Transform;
class RenderComponent;

class Renderer
{
public:
	Renderer();
	~Renderer();

	Renderer(const Renderer& other) = delete;
	Renderer& operator=(const Renderer& other) = delete;

	Renderer(Renderer&& other) = delete;
	Renderer& operator=(Renderer&& other) = delete;

	void RenderLoop();
	void Render();

private:
	using RenderComponentVector = std::vector<RenderComponent*>;
	RenderComponentVector RenderComponents;

	std::jthread RenderThread;

public:
	static void Init(SDL_Renderer* renderer);
	static void CleanUp();

	static void RenderAll();

	static void Register(RenderComponent* renderComponent);
	static void UnRegister(RenderComponent* renderComponent);

	static void RenderTexture(const Texture& texture, const Math::Vector2& pos);
	static void RenderTexture(const Texture& texture, const Math::Vector2& pos, const Math::Vector2& size);
	static void RenderTexture(const Texture& texture, Transform* transform);

	static SDL_Renderer* GetCurrentRenderer() { return RenderTarget; }

private:
	static void ApplyCamera(Transform* transform, Math::Vector2& pos, float& rot, float& scale);

#ifdef _DEBUG
	static void ApplyViewport(Transform* transform, Math::Vector2& pos, float& rot, float& scale);
#endif

private:
	static SDL_Renderer* RenderTarget;
	static Renderer* Instance;
	static std::mutex RenderLock;
	

};

