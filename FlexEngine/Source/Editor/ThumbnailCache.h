#pragma once
#include "Editor/EditorCore.h"
#include "Core/Assets/Sprite.h"
#include "Core/Assets/Texture.h"
#include "Core/Uid.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

struct ImDrawList;
struct ImVec2;

namespace Flex
{
	class AssetDatabase;
	struct AssetInfo;

	// Small previews of textures and sprites for editor windows. Loads on first use and
	// keeps the assets loaded while the editor runs; cleared when the assets are refreshed.
	class FLEX_EDITOR_API ThumbnailCache
	{
	public:
		explicit ThumbnailCache(const AssetDatabase& database) : Database{ database } {}

		// Draws the asset's preview fitted (aspect kept) and centred inside [min, max].
		// Returns false when the asset has no preview (unknown type, failed load).
		bool Draw(ImDrawList* drawList, Uid assetId, const ImVec2& min, const ImVec2& max);

		void Clear();

		// Forgets one asset's preview, so the next Draw makes it again (e.g. after a plugin rewrote the asset).
		// Textures updated in place (Texture::UpdatePixels) don't need this.
		void Invalidate(Uid assetId);

		// Previews for another asset type (registered by plugins). Called once per asset until invalidated.
		using ThumbnailProvider = std::function<std::shared_ptr<Texture>(const AssetInfo&)>;
		void SetProvider(const std::string& typeName, ThumbnailProvider provider);

	private:
		const AssetDatabase& Database;
		std::unordered_map<Uid, std::shared_ptr<Texture>> Textures;
		std::unordered_map<Uid, std::shared_ptr<Sprite>> Sprites; // share their texture with Textures
		std::unordered_map<Uid, std::shared_ptr<Texture>> Provided; // made by a provider (nullptr = none)
		std::unordered_map<std::string, ThumbnailProvider> Providers; // by asset type
	};
}
