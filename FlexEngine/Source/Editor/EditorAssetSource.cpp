#include "Editor/EditorAssetSource.h"
#include "Editor/AssetDatabase.h"
#include "Editor/TextureImporter.h"

namespace Flex
{
	std::shared_ptr<Asset> EditorAssetSource::LoadAsset(Uid id, std::string& outName, std::string& outError)
	{
		const AssetInfo* asset = Database.Find(id);
		if (!asset) {
			outError = "No asset with this id in the project (deleted, or its .meta was lost).";
			return nullptr;
		}

		outName = asset->Name;

		if (asset->Type == TextureImporter::TypeName) {
			return TextureImporter::Import(Database, *asset, outError);
		}
		if (asset->Type == TextureImporter::SpriteTypeName) {
			return TextureImporter::ImportSprite(Database, *asset, outError);
		}

		outError = "No importer for asset type '" + asset->Type + "'.";
		return nullptr;
	}

	std::vector<Uid> EditorAssetSource::GetSubAssets(Uid parentId)
	{
		std::vector<Uid> ids;
		for (const AssetInfo* subAsset : Database.GetSubAssets(parentId)) {
			ids.push_back(subAsset->Id);
		}
		return ids;
	}
}
