#pragma once
#include "Core/Assets/AssetManager.h"

namespace Flex
{
	class AssetDatabase;

	// Lets AssetManager load assets straight from the project's source files
	// (Content/ + .meta). Exported games will use a cooked-data source instead.
	class EditorAssetSource final : public AssetSource
	{
	public:
		explicit EditorAssetSource(const AssetDatabase& database) : Database{ database } {}

		std::shared_ptr<Asset> LoadAsset(Uid id, std::string& outName, std::string& outError) override;
		std::vector<Uid> GetSubAssets(Uid parentId) override;

	private:
		const AssetDatabase& Database;
	};
}
