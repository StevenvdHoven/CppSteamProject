#pragma once
#include "Core/Assets/Sprite.h"
#include "Core/Assets/Texture.h"
#include "Core/Uid.h"
#include <memory>
#include <unordered_map>

struct ImDrawList;
struct ImVec2;

namespace Flex
{
	class AssetDatabase;

	// Small previews of textures and sprites for editor windows. Loads on first use and
	// keeps the assets loaded while the editor runs; cleared when the assets are refreshed.
	class ThumbnailCache
	{
	public:
		explicit ThumbnailCache(const AssetDatabase& database) : Database{ database } {}

		// Draws the asset's preview fitted (aspect kept) and centred inside [min, max].
		// Returns false when the asset has no preview (unknown type, failed load).
		bool Draw(ImDrawList* drawList, Uid assetId, const ImVec2& min, const ImVec2& max);

		void Clear();

	private:
		const AssetDatabase& Database;
		std::unordered_map<Uid, std::shared_ptr<Texture>> Textures;
		std::unordered_map<Uid, std::shared_ptr<Sprite>> Sprites; // share their texture with Textures
	};
}
