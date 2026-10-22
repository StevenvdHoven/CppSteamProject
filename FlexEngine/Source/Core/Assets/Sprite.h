#pragma once
#include "Core/Assets/Asset.h"
#include "Core/Assets/Texture.h"
#include <cstdint>
#include <memory>
#include <string>

namespace Flex
{
	class Archive;

	// Pixel rectangle inside a texture. (0, 0) is the top-left pixel.
	struct FLEX_API SpriteRect
	{
		int32_t X = 0;
		int32_t Y = 0;
		int32_t Width = 0;
		int32_t Height = 0;

		bool operator==(const SpriteRect&) const = default;

		// Keeps the rectangle inside a textureWidth x textureHeight image, at least 1x1.
		SpriteRect ClampedTo(int32_t textureWidth, int32_t textureHeight) const;
	};

	// How a sprite is described in its texture's .meta:
	//
	//     [Sprite version=1]
	//     Id = uid(…)
	//     Name = "Archer-Idle_0"
	//     Rect.X = 0
	//     Rect.Y = 0
	//     Rect.Width = 100
	//     Rect.Height = 100
	//     Pivot.X = 0.5
	//     Pivot.Y = 1
	struct FLEX_API SpriteDefinition
	{
		static constexpr uint32_t Version = 1;

		Uid Id;
		std::string Name;
		SpriteRect Rect;

		// 0..1 inside the rectangle, y pointing down: (0.5, 0.5) = center, (0.5, 1) = bottom center.
		float PivotX = 0.5f;
		float PivotY = 0.5f;

		void Serialize(Archive& archive);

		bool operator==(const SpriteDefinition&) const = default;
	};

	// A rectangle of a shared Texture, with its own Uid.
	// Any number of sprites can use one texture: the image is in memory once,
	// and stays loaded as long as at least one sprite (or anything else) uses it.
	class FLEX_API Sprite final : public Asset
	{
	public:
		static std::shared_ptr<Sprite> Create(std::shared_ptr<Texture> texture, const SpriteDefinition& definition);

		const char* GetTypeName() const override { return "Sprite"; }
		size_t GetMemorySize() const override { return sizeof(Sprite); } // the pixels belong to the texture

		const std::shared_ptr<Texture>& GetTexture() const { return SourceTexture; }
		const SpriteRect& GetRect() const { return Rect; }
		float GetPivotX() const { return PivotX; }
		float GetPivotY() const { return PivotY; }

		// Picks up an edited rectangle/pivot without reloading (used by the editor).
		void ApplyDefinition(const SpriteDefinition& definition);

	private:
		Sprite() = default;

	private:
		std::shared_ptr<Texture> SourceTexture;
		SpriteRect Rect;
		float PivotX = 0.5f;
		float PivotY = 0.5f;
	};
}
