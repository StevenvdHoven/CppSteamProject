#include "Editor/ThumbnailCache.h"
#include "Editor/AssetDatabase.h"
#include "Editor/TextureImporter.h"
#include "Core/Assets/AssetManager.h"

#include <imgui.h>
#include <algorithm>

namespace Flex
{
	bool ThumbnailCache::Draw(ImDrawList* drawList, Uid assetId, const ImVec2& min, const ImVec2& max)
	{
		const AssetInfo* asset = Database.Find(assetId);
		if (!asset) {
			return false;
		}

		const Texture* texture = nullptr;
		float u0 = 0.0f;
		float v0 = 0.0f;
		float u1 = 1.0f;
		float v1 = 1.0f;
		float width = 0.0f;
		float height = 0.0f;

		if (asset->Type == TextureImporter::TypeName) {
			auto [entry, bInserted] = Textures.try_emplace(assetId);
			if (bInserted) {
				entry->second = AssetManager::Load<Texture>(assetId);
			}
			texture = entry->second.get();
			if (texture) {
				width = static_cast<float>(texture->GetWidth());
				height = static_cast<float>(texture->GetHeight());
			}
		} else if (asset->Type == TextureImporter::SpriteTypeName) {
			// Loading the sprite loads (or reuses) its texture: still one image in memory.
			auto [entry, bInserted] = Sprites.try_emplace(assetId);
			if (bInserted) {
				entry->second = AssetManager::Load<Sprite>(assetId);
			}
			if (const Sprite* sprite = entry->second.get()) {
				texture = sprite->GetTexture().get();
				const SpriteRect& rect = sprite->GetRect();
				const float textureWidth = static_cast<float>(texture->GetWidth());
				const float textureHeight = static_cast<float>(texture->GetHeight());
				u0 = rect.X / textureWidth;
				v0 = rect.Y / textureHeight;
				u1 = (rect.X + rect.Width) / textureWidth;
				v1 = (rect.Y + rect.Height) / textureHeight;
				width = static_cast<float>(rect.Width);
				height = static_cast<float>(rect.Height);
			}
		}

		if (!texture || width <= 0.0f || height <= 0.0f) {
			return false;
		}

		const float scale = std::min((max.x - min.x) / width, (max.y - min.y) / height);
		const ImVec2 size{ width * scale, height * scale };
		const ImVec2 imageMin{ min.x + (max.x - min.x - size.x) * 0.5f, min.y + (max.y - min.y - size.y) * 0.5f };
		drawList->AddImage(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture->GetSDLTexture())),
			imageMin, ImVec2(imageMin.x + size.x, imageMin.y + size.y), ImVec2(u0, v0), ImVec2(u1, v1));
		return true;
	}

	void ThumbnailCache::Clear()
	{
		Sprites.clear();
		Textures.clear();
	}
}
