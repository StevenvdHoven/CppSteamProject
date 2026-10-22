#include "Editor/TextureImporter.h"
#include "Editor/AssetDatabase.h"
#include "Core/Assets/AssetManager.h"
#include "Core/FileSystem.h"
#include "Core/Serialization/TextArchive.h"
#include "Core/Serialization/TextDocument.h"
#include <SDL3/SDL_log.h>
#include <algorithm>

namespace Flex::TextureImporter
{
	namespace
	{
		constexpr const char* IMPORTSECTION = "Import";
		constexpr const char* SPRITESECTION = "Sprite";
	}

	void Register(AssetDatabase& database)
	{
		// SDL 3.4 loads PNG and BMP without SDL_image.
		database.RegisterType(TypeName, { ".png", ".bmp" }, [](TextDocument& meta) {
			TextureImportSettings defaults;
			TextArchive archive(meta.FindOrAddSection(IMPORTSECTION), ArchiveMode::Save);
			defaults.Serialize(archive);
		});
		database.RegisterSubAssetType(TypeName, SPRITESECTION, SpriteTypeName);
	}

	bool ReadSettings(const AssetDatabase& database, const AssetInfo& asset, TextureImportSettings& outSettings, std::string& outError)
	{
		TextDocument meta;
		if (!database.LoadMeta(asset, meta, outError)) {
			return false;
		}

		if (TextSection* section = meta.FindSection(IMPORTSECTION)) {
			TextArchive archive(*section, ArchiveMode::Load);
			outSettings.Serialize(archive);
			for (const std::string& warning : archive.GetWarnings()) {
				SDL_Log("%s.meta: %s", PathToUtf8(asset.Path).c_str(), warning.c_str());
			}
		}
		return true;
	}

	bool WriteSettings(const AssetDatabase& database, const AssetInfo& asset, const TextureImportSettings& settings, std::string& outError)
	{
		// Load first so [Meta], sprites (later) and comments are kept.
		TextDocument meta;
		if (!database.LoadMeta(asset, meta, outError)) {
			return false;
		}

		TextureImportSettings copy = settings;
		TextArchive archive(meta.FindOrAddSection(IMPORTSECTION), ArchiveMode::Save);
		copy.Serialize(archive);
		return database.SaveMeta(asset, meta, outError);
	}

	std::shared_ptr<Texture> Import(const AssetDatabase& database, const AssetInfo& asset, std::string& outError)
	{
		TextureImportSettings settings;
		if (!ReadSettings(database, asset, settings, outError)) {
			return nullptr;
		}
		return Texture::LoadFromFile(database.GetSourcePath(asset), settings, outError);
	}

	bool ReadSprites(const AssetDatabase& database, const AssetInfo& texture, std::vector<SpriteDefinition>& outSprites, std::string& outError)
	{
		TextDocument meta;
		if (!database.LoadMeta(texture, meta, outError)) {
			return false;
		}

		outSprites.clear();
		for (TextSection& section : meta.Sections)
		{
			if (section.Type != SPRITESECTION) {
				continue;
			}
			SpriteDefinition sprite;
			TextArchive archive(section, ArchiveMode::Load);
			sprite.Serialize(archive);
			for (const std::string& warning : archive.GetWarnings()) {
				SDL_Log("%s.meta: %s", PathToUtf8(texture.Path).c_str(), warning.c_str());
			}
			outSprites.push_back(std::move(sprite));
		}
		return true;
	}

	bool WriteSprites(const AssetDatabase& database, const AssetInfo& texture, const std::vector<SpriteDefinition>& sprites, std::string& outError)
	{
		TextDocument meta;
		if (!database.LoadMeta(texture, meta, outError)) {
			return false;
		}

		std::erase_if(meta.Sections, [](const TextSection& section) { return section.Type == SPRITESECTION; });
		for (const SpriteDefinition& sprite : sprites)
		{
			SpriteDefinition copy = sprite;
			TextArchive archive(meta.AddSection(SPRITESECTION), ArchiveMode::Save);
			copy.Serialize(archive);
		}
		return database.SaveMeta(texture, meta, outError);
	}

	std::shared_ptr<Sprite> ImportSprite(const AssetDatabase& database, const AssetInfo& spriteAsset, std::string& outError)
	{
		const AssetInfo* textureAsset = database.Find(spriteAsset.ParentId);
		if (!textureAsset) {
			outError = "The sprite's texture is not in the project.";
			return nullptr;
		}

		std::vector<SpriteDefinition> sprites;
		if (!ReadSprites(database, *textureAsset, sprites, outError)) {
			return nullptr;
		}

		const auto definition = std::find_if(sprites.begin(), sprites.end(), [&](const SpriteDefinition& sprite) { return sprite.Id == spriteAsset.Id; });
		if (definition == sprites.end()) {
			outError = "Sprite not found in " + PathToUtf8(textureAsset->Path) + ".meta (re-sliced or deleted?).";
			return nullptr;
		}

		// Shared: every sprite of this texture gets the very same Texture object.
		std::shared_ptr<Texture> texture = AssetManager::Load<Texture>(textureAsset->Id);
		if (!texture) {
			outError = "Could not load the sprite's texture " + PathToUtf8(textureAsset->Path) + ".";
			return nullptr;
		}
		return Sprite::Create(std::move(texture), *definition);
	}
}
