#include "Core/Assets/Sprite.h"
#include "Core/Serialization/Archive.h"
#include <algorithm>

namespace Flex
{
	SpriteRect SpriteRect::ClampedTo(int32_t textureWidth, int32_t textureHeight) const
	{
		SpriteRect result;
		const int32_t maxX = std::max(textureWidth - 1, 0);
		const int32_t maxY = std::max(textureHeight - 1, 0);
		result.X = std::clamp(X, 0, maxX);
		result.Y = std::clamp(Y, 0, maxY);
		result.Width = std::clamp(Width, 1, std::max(textureWidth - result.X, 1));
		result.Height = std::clamp(Height, 1, std::max(textureHeight - result.Y, 1));
		return result;
	}

	void SpriteDefinition::Serialize(Archive& archive)
	{
		archive.SerializeVersion(Version);
		archive.Field("Id", Id);
		archive.Field("Name", Name);

		archive.BeginObject("Rect");
		archive.Field("X", Rect.X);
		archive.Field("Y", Rect.Y);
		archive.Field("Width", Rect.Width);
		archive.Field("Height", Rect.Height);
		archive.EndObject();

		archive.BeginObject("Pivot");
		archive.Field("X", PivotX);
		archive.Field("Y", PivotY);
		archive.EndObject();
	}

	std::shared_ptr<Sprite> Sprite::Create(std::shared_ptr<Texture> texture, const SpriteDefinition& definition)
	{
		if (!texture) {
			return nullptr;
		}

		auto sprite = std::shared_ptr<Sprite>(new Sprite());
		sprite->SourceTexture = std::move(texture);
		sprite->ApplyDefinition(definition);
		return sprite;
	}

	void Sprite::ApplyDefinition(const SpriteDefinition& definition)
	{
		Rect = definition.Rect.ClampedTo(SourceTexture->GetWidth(), SourceTexture->GetHeight());
		PivotX = definition.PivotX;
		PivotY = definition.PivotY;
	}
}
