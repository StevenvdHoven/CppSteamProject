#pragma once
#include "Editor/EditorWindow.h"
#include "Core/Uid.h"
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace Flex
{
	struct AssetInfo;
	struct EditorContext;

	// The "Content" window: folders of Content/ on the left, the assets of the chosen folder
	// as thumbnail tiles on the right. Click selects (Inspector), double-click opens the
	// asset's editor, and tiles can be dragged into other windows (payload AssetPayloadType).
	// Textures with sprites get a small arrow that shows their sprites as tiles too.
	class ContentBrowser final : public EditorWindow
	{
	public:
		explicit ContentBrowser(EditorContext& context);

		void Draw() override;

	private:
		struct Folder
		{
			std::string Path; // generic, relative to Content/ ("" = Content itself)
			std::string Name;
			std::vector<size_t> Children;
		};

		void RebuildFolders();
		void DrawToolbar();
		void DrawFolderTree(size_t folderIndex);
		void DrawTiles();
		void DrawTile(const AssetInfo& asset, float tileSize, bool bIsSubAsset);
		bool MatchesSearch(const AssetInfo& asset) const;

	private:
		EditorContext& Context;
		std::vector<Folder> Folders; // [0] is Content/
		uint32_t FoldersRevision = UINT32_MAX;
		std::string CurrentFolder;
		char SearchText[128]{};
		float TileSize = 72.0f;
		std::unordered_set<Uid> ExpandedTextures;
	};
}
