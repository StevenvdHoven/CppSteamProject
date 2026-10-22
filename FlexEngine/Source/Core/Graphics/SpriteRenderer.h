#pragma once
#include "Core/Assets/Asset.h"
#include "Core/Assets/Sprite.h"
#include "Core/Assets/Texture.h"
#include "Core/Math/Color.h"
#include "Core/Math/Rect.h"
#include "Core/Serialization/Archive.h"
#include "Core/Uid.h"
#include "Core/World/ComponentRegistery.h"
#include <cstdint>
#include <memory>

namespace Flex
{
	// What a SpriteRenderer draws: texture, uv and the quad in the entity's local space
	// (pivot at the origin, in pixels).
	struct SpriteDrawInfo
	{
		const Texture* DrawTexture = nullptr;
		Rect LocalRect;
		Rect UV{ 0.0f, 0.0f, 1.0f, 1.0f };
	};

	// Draws a sprite (or a whole texture) at the entity's WorldTransform.
	// Size = the sprite's pixel size times the transform's scale (1 world unit = 1 pixel).
	struct SpriteRenderer
	{
		FLEX_COMPONENT(SpriteRenderer)
		static constexpr uint32_t Version = 1;

		Uid AssetId;               // a Sprite or a Texture
		Color Tint = Color::White();
		int32_t Order = 0;         // higher draws on top; equal orders keep their draw order

		void Serialize(Archive& archive)
		{
			archive.SerializeVersion(Version);
			archive.Field("Asset", AssetId, { Sprite::TypeName, Texture::TypeName });
			archive.Field("Tint", Tint);
			archive.Field("Order", Order);
		}

		// Runtime cache (not saved): the asset loaded for LoadedId and its resolved quad.
		// Filled by the sprite renderer; ResetSpriteCaches clears it after assets changed.
		std::shared_ptr<Asset> Loaded;
		Uid LoadedId;
		SpriteDrawInfo CachedInfo;
		bool bHasCachedInfo = false;
	};
}
