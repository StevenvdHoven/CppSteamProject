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

	Math::Vector2 ViewportToWorld(const Math::Vector2& vec);

	Math::Matrix2x3 GetViewportMatrix() const {
		return ViewMatrix;
	}

private:
	void Resize(int width, int height);
	void HandleInput();

private:
	int Width = 0;
	int Height = 0;

	Math::Vector2 Offset;
	float Scale = 1.0f;

	Math::Matrix2x3 ViewMatrix;
	Math::Matrix2x3 ViewMatrixInverse;

	SDL_Renderer* Renderer = nullptr;
	SDL_Texture* ViewportTexture = nullptr;
};
#endif