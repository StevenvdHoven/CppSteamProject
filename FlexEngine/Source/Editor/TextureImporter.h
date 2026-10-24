#pragma once
#include "Editor/EditorCore.h"
#include "Core/Assets/Sprite.h"
#include "Core/Assets/Texture.h"
#include <memory>
#include <string>
#include <vector>

namespace Flex
{
	class AssetDatabase;
	struct AssetInfo;

	// Editor-side knowledge about texture source files (.png/.bmp + their .meta).
	namespace TextureImporter
	{
		constexpr const char* TypeName = Texture::TypeName;
		constexpr const char* SpriteTypeName = Sprite::TypeName;

		// Registers the extensions, the default [Import] settings for new .meta files,
		// and [Sprite] sections as sub-assets.
		FLEX_EDITOR_API void Register(AssetDatabase& database);

		// [Import] section of the .meta. Missing settings keep their defaults.
		FLEX_EDITOR_API bool ReadSettings(const AssetDatabase& database, const AssetInfo& asset, TextureImportSettings& outSettings, std::string& outError);
		FLEX_EDITOR_API bool WriteSettings(const AssetDatabase& database, const AssetInfo& asset, const TextureImportSettings& settings, std::string& outError);

		FLEX_EDITOR_API std::shared_ptr<Texture> Import(const AssetDatabase& database, const AssetInfo& asset, std::string& outError);

		// All [Sprite] sections of a texture's .meta, in file order.
		FLEX_EDITOR_API bool ReadSprites(const AssetDatabase& database, const AssetInfo& texture, std::vector<SpriteDefinition>& outSprites, std::string& outError);

		// Replaces all [Sprite] sections; everything else in the .meta is kept.
		FLEX_EDITOR_API bool WriteSprites(const AssetDatabase& database, const AssetInfo& texture, const std::vector<SpriteDefinition>& sprites, std::string& outError);

		// spriteAsset is the sub-asset entry of the sprite; its texture is loaded through AssetManager (shared).
		FLEX_EDITOR_API std::shared_ptr<Sprite> ImportSprite(const AssetDatabase& database, const AssetInfo& spriteAsset, std::string& outError);
	}
}
