#pragma once
#include "EditorWindow.h"
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

private:
	void Resize(int width, int height);

private:
	SDL_Renderer* Renderer = nullptr;
	SDL_Texture* ViewportTexture = nullptr;

	int Width = 0;
	int Height = 0;
};

