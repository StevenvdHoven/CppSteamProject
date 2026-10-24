#include "Core/Assets/ContentAssetSource.h"
#include "Core/FileSystem.h"
#include "Core/Serialization/TextArchive.h"
#include "Core/Serialization/TextDocument.h"
#include <SDL3/SDL.h>

namespace Flex
{
	namespace
	{
		constexpr const char* MetaExtension = ".meta";
	}

	ContentAssetSource::ContentAssetSource(const std::filesystem::path& contentDirectory)
	{
		std::error_code error;
		for (auto it = std::filesystem::recursive_directory_iterator(contentDirectory, error); !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
			if (!it->is_regular_file(error) || it->path().extension() != MetaExtension) {
				continue;
			}
			TextDocument meta;
			std::string loadError;
			if (!meta.Load(it->path(), loadError)) {
				SDL_Log("ContentAssetSource: %s", loadError.c_str());
				continue;
			}
			TextSection* metaSection = meta.FindSection("Meta");
			if (!metaSection) {
				continue;
			}

			Entry entry;
			Uid id;
			{
				TextArchive archive(*metaSection, ArchiveMode::Load);
				archive.Field("Id", id);
				archive.Field("Type", entry.Type);
			}
			if (!id.IsValid()) {
				continue;
			}
			std::filesystem::path sourceFile = it->path();
			sourceFile.replace_extension(); // "x.png.meta" -> "x.png"
			entry.File = sourceFile;
			entry.Name = PathToUtf8(sourceFile.stem());

			for (TextSection& section : meta.Sections) {
				if (section.Type == "Import" && entry.Type == Texture::TypeName) {
					TextArchive archive(section, ArchiveMode::Load);
					entry.Import.Serialize(archive);
				} else if (section.Type == "Sprite" && entry.Type == Texture::TypeName) {
					Entry sprite;
					TextArchive archive(section, ArchiveMode::Load);
					sprite.Definition.Serialize(archive);
					if (!sprite.Definition.Id.IsValid()) {
						continue;
					}
					sprite.File = sourceFile;
					sprite.Type = Sprite::TypeName;
					sprite.Name = sprite.Definition.Name;
					sprite.ParentId = id;
					SubAssets[id].push_back(sprite.Definition.Id);
					Entries[sprite.Definition.Id] = std::move(sprite);
				}
			}
			Entries[id] = std::move(entry);
		}
	}

	std::shared_ptr<Asset> ContentAssetSource::LoadAsset(Uid id, std::string& outName, std::string& outError)
	{
		const auto it = Entries.find(id);
		if (it == Entries.end()) {
			outError = "No asset with this id in Content/.";
			return nullptr;
		}
		const Entry& entry = it->second;
		outName = entry.Name;

		if (entry.Type == Texture::TypeName) {
			return Texture::LoadFromFile(entry.File, entry.Import, outError);
		}
		if (entry.Type == Sprite::TypeName) {
			// Shared: every sprite of this texture gets the very same Texture object.
			std::shared_ptr<Texture> texture = AssetManager::Load<Texture>(entry.ParentId);
			if (!texture) {
				outError = "Could not load the sprite's texture " + PathToUtf8(entry.File) + ".";
				return nullptr;
			}
			return Sprite::Create(std::move(texture), entry.Definition);
		}
		if (AssetManager::HasFileLoader(entry.Type)) {
			return AssetManager::LoadFromFile(entry.Type, entry.File, outError);
		}
		outError = "Assets of type '" + entry.Type + "' can't be loaded by the runtime yet.";
		return nullptr;
	}

	std::vector<Uid> ContentAssetSource::GetSubAssets(Uid parentId)
	{
		const auto it = SubAssets.find(parentId);
		return it != SubAssets.end() ? it->second : std::vector<Uid>{};
	}

	std::filesystem::path ContentAssetSource::GetSourcePath(Uid id) const
	{
		const auto it = Entries.find(id);
		return it != Entries.end() ? it->second.File : std::filesystem::path{};
	}
}
