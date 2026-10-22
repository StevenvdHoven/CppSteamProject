#include "Core/Graphics/SpriteBatch.h"
#include "Core/Graphics/Renderer.h"
#include <SDL3/SDL.h>
#include <cstddef>

namespace Flex
{
	static_assert(sizeof(SpriteVertex) == sizeof(SDL_Vertex), "SpriteVertex must match SDL_Vertex.");
	static_assert(offsetof(SpriteVertex, Position) == offsetof(SDL_Vertex, position), "SpriteVertex must match SDL_Vertex.");
	static_assert(offsetof(SpriteVertex, Tint) == offsetof(SDL_Vertex, color), "SpriteVertex must match SDL_Vertex.");
	static_assert(offsetof(SpriteVertex, TexCoord) == offsetof(SDL_Vertex, tex_coord), "SpriteVertex must match SDL_Vertex.");

	SpriteBatch::SpriteBatch()
	{
		Vertices.resize(static_cast<size_t>(MaxQuadsPerDrawCall) * 4);
		Indices.resize(static_cast<size_t>(MaxQuadsPerDrawCall) * 6);
		for (uint32_t quad = 0; quad < MaxQuadsPerDrawCall; ++quad) {
			const int first = static_cast<int>(quad * 4);
			int* index = Indices.data() + static_cast<size_t>(quad) * 6;
			index[0] = first;
			index[1] = first + 1;
			index[2] = first + 2;
			index[3] = first;
			index[4] = first + 2;
			index[5] = first + 3;
		}
	}

	void SpriteBatch::Flush()
	{
		SubmitQueued();
	}

	void SpriteBatch::ResetStatistics()
	{
		DrawCallCount = 0;
		QuadCount = 0;
	}

	void SpriteBatch::SubmitQueued()
	{
		if (QueuedQuads == 0) {
			return;
		}
		FLEX_ASSERT(Renderer::IsInitialised(), "SpriteBatch used without a renderer.");
		SDL_RenderGeometry(Renderer::GetSDLRenderer(), CurrentTexture,
			reinterpret_cast<const SDL_Vertex*>(Vertices.data()), static_cast<int>(QueuedQuads * 4),
			Indices.data(), static_cast<int>(QueuedQuads * 6));
		++DrawCallCount;
		QuadCount += QueuedQuads;
		QueuedQuads = 0;
	}
}
