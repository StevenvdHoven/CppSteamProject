#pragma once
#include "Core/Core.h"
#include "Core/Math/Color.h"
#include "Core/Math/Matrix2x3.h"
#include "Core/Math/Rect.h"
#include "Core/World/Entity.h"
#include <vector>

namespace Flex
{
	class SpriteBatch;
	class Texture;
	class World;
	struct SpriteRenderer;

	// What a SpriteRenderer draws: texture, uv and the quad in the entity's local space
	// (pivot at the origin, in pixels).
	struct SpriteDrawInfo
	{
		const Texture* DrawTexture = nullptr;
		Rect LocalRect;
		Rect UV{ 0.0f, 0.0f, 1.0f, 1.0f };
	};

	// Loads the renderer's asset if needed (cached in the component) and describes the quad.
	// False when there's nothing to draw (no asset, unknown type, failed load).
	FLEX_API bool ResolveSprite(SpriteRenderer& renderer, SpriteDrawInfo& outInfo);

	// One visible sprite, in draw order.
	struct SpriteDrawItem
	{
		Entity Owner;
		int32_t Order = 0;
		SpriteDrawInfo Info;
		Matrix2x3 World;
		Color Tint;
	};

	// Every entity with WorldTransform + SpriteRenderer, sorted by Order (stable, so equal
	// orders keep a consistent order). Also used for picking: the last item is on top.
	FLEX_API void CollectSprites(World& world, std::vector<SpriteDrawItem>& outItems);

	// Draws the collected sprites through the batch; view maps world to target pixels.
	FLEX_API void DrawSprites(const std::vector<SpriteDrawItem>& items, SpriteBatch& batch, const Matrix2x3& view);

	// Forgets every cached asset (e.g. after an asset refresh), so the next draw reloads.
	FLEX_API void ResetSpriteCaches(World& world);
}
