#pragma once
#include "Core/Core.h"
#include "Core/Assets/Texture.h"
#include "Core/Math/Color.h"
#include "Core/Math/Matrix2x3.h"
#include "Core/Math/Rect.h"
#include "Core/Math/Vector2.h"
#include <cstdint>
#include <vector>

struct SDL_Texture;

namespace Flex
{
	// Same memory layout as SDL_Vertex (checked in SpriteBatch.cpp).
	struct SpriteVertex
	{
		Vector2 Position;
		Color Tint;
		Vector2 TexCoord;
	};

	// Collects quads and draws them with as few draw calls as possible
	// (one per run of quads that use the same texture). Draw calls go through
	// Renderer's SDL_Renderer, so a batch must not outlive Renderer::Shutdown.
	//
	//     batch.Draw(ballTexture.get(), Rect::FromCenter(position, { 8, 8 }), Color::White());
	//     batch.DrawRect(playerRect, Color{ 1, 0.5f, 0 });
	//     batch.Flush();   // before SDL_RenderPresent
	//
	// Quads are drawn in the order they were added. Draw is inline, so filling
	// the batch from a game loop doesn't call into FlexCore per quad.
	class FLEX_API SpriteBatch
	{
	public:
		static constexpr uint32_t MaxQuadsPerDrawCall = 32768;

		SpriteBatch();

		// texture may be nullptr for a solid quad. uv is in 0-1 texture space.
		void Draw(const Texture* texture, const Rect& destination, const Color& tint, const Rect& uv = Rect{ 0.0f, 0.0f, 1.0f, 1.0f })
		{
			SDL_Texture* handle = texture ? texture->GetSDLTexture() : nullptr;
			if (handle != CurrentTexture || QueuedQuads == MaxQuadsPerDrawCall) {
				SubmitQueued();
				CurrentTexture = handle;
			}

			const float left = destination.x;
			const float top = destination.y;
			const float right = destination.x + destination.width;
			const float bottom = destination.y + destination.height;
			const float uvRight = uv.x + uv.width;
			const float uvBottom = uv.y + uv.height;

			SpriteVertex* vertex = Vertices.data() + static_cast<size_t>(QueuedQuads) * 4;
			vertex[0] = { { left, top }, tint, { uv.x, uv.y } };
			vertex[1] = { { right, top }, tint, { uvRight, uv.y } };
			vertex[2] = { { right, bottom }, tint, { uvRight, uvBottom } };
			vertex[3] = { { left, bottom }, tint, { uv.x, uvBottom } };
			++QueuedQuads;
		}

		void DrawRect(const Rect& destination, const Color& color)
		{
			Draw(nullptr, destination, color);
		}

		// A quad that can be rotated, scaled or mirrored: localRect's corners go through transform.
		void DrawTransformed(const Texture* texture, const Matrix2x3& transform, const Rect& localRect, const Color& tint, const Rect& uv = Rect{ 0.0f, 0.0f, 1.0f, 1.0f })
		{
			SDL_Texture* handle = texture ? texture->GetSDLTexture() : nullptr;
			if (handle != CurrentTexture || QueuedQuads == MaxQuadsPerDrawCall) {
				SubmitQueued();
				CurrentTexture = handle;
			}

			const float right = localRect.x + localRect.width;
			const float bottom = localRect.y + localRect.height;
			const float uvRight = uv.x + uv.width;
			const float uvBottom = uv.y + uv.height;

			SpriteVertex* vertex = Vertices.data() + static_cast<size_t>(QueuedQuads) * 4;
			vertex[0] = { transform.TransformPoint({ localRect.x, localRect.y }), tint, { uv.x, uv.y } };
			vertex[1] = { transform.TransformPoint({ right, localRect.y }), tint, { uvRight, uv.y } };
			vertex[2] = { transform.TransformPoint({ right, bottom }), tint, { uvRight, uvBottom } };
			vertex[3] = { transform.TransformPoint({ localRect.x, bottom }), tint, { uv.x, uvBottom } };
			++QueuedQuads;
		}

		// Draws everything queued. Call before presenting the frame.
		void Flush();

		// Statistics since the last ResetStatistics.
		uint32_t GetDrawCallCount() const { return DrawCallCount; }
		uint32_t GetQuadCount() const { return QuadCount; }
		void ResetStatistics();

	private:
		void SubmitQueued();

		std::vector<SpriteVertex> Vertices; // MaxQuadsPerDrawCall * 4, allocated once
		std::vector<int> Indices;           // MaxQuadsPerDrawCall * 6, the same for every batch
		SDL_Texture* CurrentTexture = nullptr;
		uint32_t QueuedQuads = 0;
		uint32_t DrawCallCount = 0;
		uint32_t QuadCount = 0;
	};
}
