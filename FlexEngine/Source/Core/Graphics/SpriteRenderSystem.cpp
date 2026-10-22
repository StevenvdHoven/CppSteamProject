#include "Core/Graphics/SpriteRenderSystem.h"
#include "Core/Assets/AssetManager.h"
#include "Core/Assets/Sprite.h"
#include "Core/Assets/Texture.h"
#include "Core/Graphics/SpriteBatch.h"
#include "Core/Graphics/SpriteRenderer.h"
#include "Core/World/Transform.h"
#include "Core/World/World.h"
#include <algorithm>

namespace Flex
{
	FLEX_REGISTER_COMPONENT(SpriteRenderer)

	namespace
	{
		bool ResolveLoaded(const SpriteRenderer& renderer, SpriteDrawInfo& outInfo);
	}

	bool ResolveSprite(SpriteRenderer& renderer, SpriteDrawInfo& outInfo)
	{
		if (renderer.LoadedId != renderer.AssetId) {
			renderer.LoadedId = renderer.AssetId;
			renderer.Loaded = renderer.AssetId.IsValid() ? AssetManager::LoadAsset(renderer.AssetId) : nullptr;
			renderer.bHasCachedInfo = renderer.Loaded && ResolveLoaded(renderer, renderer.CachedInfo);
		}
		if (!renderer.bHasCachedInfo) {
			return false;
		}
		outInfo = renderer.CachedInfo;
		return true;
	}

	namespace
	{
		bool ResolveLoaded(const SpriteRenderer& renderer, SpriteDrawInfo& outInfo)
		{
			if (const Sprite* sprite = dynamic_cast<const Sprite*>(renderer.Loaded.get())) {
				const Texture* texture = sprite->GetTexture().get();
				const SpriteRect& rect = sprite->GetRect();
				if (!texture || rect.Width <= 0 || rect.Height <= 0) {
					return false;
				}
				const float width = static_cast<float>(rect.Width);
				const float height = static_cast<float>(rect.Height);
				const float textureWidth = static_cast<float>(texture->GetWidth());
				const float textureHeight = static_cast<float>(texture->GetHeight());
				outInfo.DrawTexture = texture;
				outInfo.LocalRect = { -sprite->GetPivotX() * width, -sprite->GetPivotY() * height, width, height };
				outInfo.UV = { rect.X / textureWidth, rect.Y / textureHeight, width / textureWidth, height / textureHeight };
				return true;
			}
			if (const Texture* texture = dynamic_cast<const Texture*>(renderer.Loaded.get())) {
				const float width = static_cast<float>(texture->GetWidth());
				const float height = static_cast<float>(texture->GetHeight());
				outInfo.DrawTexture = texture;
				outInfo.LocalRect = { -width * 0.5f, -height * 0.5f, width, height };
				outInfo.UV = { 0.0f, 0.0f, 1.0f, 1.0f };
				return true;
			}
			return false;
		}
	}

	void CollectSprites(World& world, std::vector<SpriteDrawItem>& outItems)
	{
		outItems.clear();
		world.Each<const WorldTransform, SpriteRenderer>([&](Entity entity, const WorldTransform& worldTransform, SpriteRenderer& renderer) {
			SpriteDrawItem item;
			if (ResolveSprite(renderer, item.Info)) {
				item.Owner = entity;
				item.Order = renderer.Order;
				item.World = worldTransform.Matrix;
				item.Tint = renderer.Tint;
				outItems.push_back(item);
			}
		});
		// Usually every sprite has the same Order: then there's nothing to sort.
		const auto byOrder = [](const SpriteDrawItem& a, const SpriteDrawItem& b) { return a.Order < b.Order; };
		if (!std::is_sorted(outItems.begin(), outItems.end(), byOrder)) {
			std::stable_sort(outItems.begin(), outItems.end(), byOrder);
		}
	}

	void DrawSprites(const std::vector<SpriteDrawItem>& items, SpriteBatch& batch, const Matrix2x3& view)
	{
		for (const SpriteDrawItem& item : items) {
			batch.DrawTransformed(item.Info.DrawTexture, view * item.World, item.Info.LocalRect, item.Tint, item.Info.UV);
		}
	}

	void ResetSpriteCaches(World& world)
	{
		world.Each<SpriteRenderer>([](SpriteRenderer& renderer) {
			renderer.Loaded.reset();
			renderer.LoadedId = Uid();
			renderer.bHasCachedInfo = false;
		});
	}
}
