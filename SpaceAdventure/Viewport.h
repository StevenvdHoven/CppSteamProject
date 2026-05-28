#ifdef _DEBUG
#pragma once
#include "EditorWindow.h"
#include "Math.h"
#include <imgui/ImGuizmo.h>

struct SDL_Texture;
struct SDL_Renderer;

class Viewport final : public EditorWindow
{
public:
	Viewport(SDL_Renderer* renderPtr, int windowWidth = 1280, int windowHeight = 720);
	~Viewport();

	void BeginFrame();   // call before rendering into it
	void EndFrame();     // call after rendering into it
	void ImGui_OnWindow() override;

	void EditTransform(float* cameraView, float* cameraProjection, float* matrix);

	SDL_Texture* GetTexture() const { return ViewportTexture; }
	SDL_Renderer* GetRenderer() const { return Renderer; }

	int GetWidth() const { return Width; }
	int GetHeight() const { return Height; }

	Math::Matrix2x3 GetViewportMatrix() const {
		return Math::Matrix2x3::FromTranslation(Offset + Math::Vector2(Width * 0.5f, Height * 0.5f)) * Math::Matrix2x3::FromScale(Scale);
	}

private:
	void Resize(int width, int height);
	void HandleInput();

private:
	int Width = 0;
	int Height = 0;

	Math::Vector2 Offset;
	float Scale = 1.0f;

	SDL_Renderer* Renderer = nullptr;
	SDL_Texture* ViewportTexture = nullptr;
};
#endif