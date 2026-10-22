#pragma once
#include "Editor/EditorWindow.h"
#include "Core/Graphics/Camera2D.h"
#include "Core/Graphics/SpriteBatch.h"
#include "Core/Graphics/SpriteRenderSystem.h"
#include "Core/Math/Vector2.h"
#include "Core/World/Entity.h"
#include <vector>

struct SDL_Texture;
struct ImDrawList;

namespace Flex
{
	struct EditorContext;

	// The "Viewport" window: the edited scene drawn into a render target, with a grid.
	//  - left click selects (sprites, or the cross of an empty GameObject); drag moves the selection
	//  - right or middle drag pans, the wheel zooms around the mouse, F frames the selection
	//  - drop a texture/sprite from Content to create a GameObject at that spot; Delete deletes
	class ViewportWindow final : public EditorWindow
	{
	public:
		explicit ViewportWindow(EditorContext& context);
		~ViewportWindow() override;

		void Draw() override;

	private:
		void DrawToolbar();
		void EnsureRenderTarget(int width, int height);
		void RenderScene(const Vector2& size, float pixelScale);
		void DrawGrid(const Vector2& size, float pixelScale);
		void HandleInput(const ImVec2& imageMin, const Vector2& size);
		void DrawOverlay(ImDrawList* drawList, const ImVec2& imageMin, const Vector2& size);
		Entity Pick(const Vector2& screenPosition, const Vector2& size);
		void FrameSelection();

	private:
		EditorContext& Context;
		Camera2D Camera;

		SDL_Texture* Target = nullptr;
		int TargetWidth = 0;
		int TargetHeight = 0;

		SpriteBatch Batch;
		std::vector<SpriteDrawItem> Items; // this frame's sprites in draw order (for picking too)

		Entity MovingEntity;
		Vector2 MoveOffset;      // entity world position - mouse world position
		Vector2 MoveStartMouse;
		bool bMoving = false;
	};
}
