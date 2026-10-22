#pragma once
#include "Core/Assets/AssetManager.h"
#include "Core/Assets/Sprite.h"
#include "Core/Assets/Texture.h"
#include "Core/Uid.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Flex
{
	// Loads assets straight from a project's Content/ folder: source files plus their .meta.
	// Read-only (it never writes .meta files, unlike the editor's AssetDatabase), for
	// FlexRuntime running a project during development. Exported games will read cooked
	// data instead.
	class FLEX_API ContentAssetSource final : public AssetSource
	{
	public:
		// Scans every .meta under contentDirectory.
		explicit ContentAssetSource(const std::filesystem::path& contentDirectory);

		std::shared_ptr<Asset> LoadAsset(Uid id, std::string& outName, std::string& outError) override;
		std::vector<Uid> GetSubAssets(Uid parentId) override;

		// The source file of an asset (a sprite gives its texture's file); empty if unknown.
		std::filesystem::path GetSourcePath(Uid id) const;
		size_t GetAssetCount() const { return Entries.size(); }

	private:
		struct Entry
		{
			std::filesystem::path File;
			std::string Type;
			std::string Name;
			Uid ParentId;
			TextureImportSettings Import;  // textures
			SpriteDefinition Definition;   // sprites
		};

		std::unordered_map<Uid, Entry> Entries;
		std::unordered_map<Uid, std::vector<Uid>> SubAssets;
	};
}
