#pragma once
#include "Editor/EditorCore.h"
#include "Core/Uid.h"
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Flex
{
	class TextDocument;

	enum class AssetTypeFlags : uint8_t
	{
		None = 0,
		EditorOnly = 1 << 0, // source files only the editor uses (e.g. layered paintings): never copied into builds
	};

	struct AssetInfo
	{
		Uid Id;
		std::string Type;
		std::filesystem::path Path; // relative to Content/ (a sub-asset has its parent's path)
		std::string Name;           // file name without extension, or the sub-asset's own name
		Uid ParentId;               // set for sub-assets (e.g. a sprite -> its texture)

		bool IsSubAsset() const { return ParentId.IsValid(); }
	};

	struct AssetRefreshReport
	{
		int AssetCount = 0;
		int MetaCreated = 0;
		int DuplicatesFixed = 0;
		int OrphanMetas = 0;
		int Errors = 0;
		std::vector<std::string> Messages;
	};

	// Editor-side index of everything in the project's Content/ folder.
	//
	// Every source file of a registered type (e.g. Archer.png) gets a sidecar
	// Archer.png.meta that holds its permanent Uid. References between assets
	// use that Uid, so renaming or moving a file (together with its .meta)
	// never breaks anything.
	//
	//     ; FlexEngine asset metadata. Keep this file next to Archer.png ...
	//     [Meta version=1]
	//     Id = uid(a41f09c3d2e85b70)
	//     Type = "Texture"
	//
	// A .meta can also hold SUB-ASSETS (e.g. [Sprite] sections in a texture's .meta).
	// Each has its own Id and Name and is listed right after its parent.
	// Other sections (import settings) are kept as-is.
	//
	// Duplicate ids (a file copied together with its .meta) are fixed by giving the COPY
	// new ids. The database remembers which file owned which id at the last refresh
	// (in memory and in an optional cache file), so the original keeps its ids even when
	// the copy's path sorts first. Without any history, the first path alphabetically wins.
	class FLEX_EDITOR_API AssetDatabase
	{
	public:
		static constexpr const char* MetaExtension = ".meta";
		static constexpr uint32_t MetaVersion = 1;

		// Called when a new .meta is created, to write the type's default import settings into it.
		using MetaInitializer = std::function<void(TextDocument& meta)>;

		// extensions like ".png" (case-insensitive).
		void RegisterType(const std::string& typeName, const std::vector<std::string>& extensions, MetaInitializer initializeMeta = {}, AssetTypeFlags flags = AssetTypeFlags::None);

		// Every [sectionType] section in the .meta of a parentType asset becomes a sub-asset
		// of type subAssetType. The section must hold "Id" and "Name" fields.
		void RegisterSubAssetType(const std::string& parentType, const std::string& sectionType, const std::string& subAssetType);

		// Per-machine file that remembers id owners between editor sessions (e.g. Saved/AssetOwners.cache).
		// Loads it now and rewrites it after every refresh.
		void SetOwnerCacheFile(const std::filesystem::path& cacheFile);

		// Scans the folder, creates missing .meta files and fixes duplicate ids.
		// Never deletes anything: orphaned .meta files are only reported.
		AssetRefreshReport Refresh(const std::filesystem::path& contentDirectory);

		const AssetInfo* Find(Uid id) const;
		const AssetInfo* FindByPath(const std::filesystem::path& relativePath) const; // file assets only
		std::vector<const AssetInfo*> GetSubAssets(Uid parentId) const;
		const std::vector<AssetInfo>& GetAssets() const { return Assets; }

		const std::string* GetTypeForExtension(const std::filesystem::path& extension) const;

		// Lower-case extensions (".flexpaint") of the EditorOnly types: Build Project skips these files and their .meta.
		const std::vector<std::string>& GetEditorOnlyExtensions() const { return EditorOnlyExtensions; }
		bool IsEditorOnly(const AssetInfo& asset) const;

		const std::filesystem::path& GetContentDirectory() const { return ContentDirectory; }
		std::filesystem::path GetSourcePath(const AssetInfo& asset) const { return ContentDirectory / asset.Path; }

		// Read or rewrite an asset's .meta (e.g. after changing import settings in the editor).
		bool LoadMeta(const AssetInfo& asset, TextDocument& outMeta, std::string& outError) const;
		bool SaveMeta(const AssetInfo& asset, const TextDocument& meta, std::string& outError) const;

	private:
		void SaveOwnerCache() const;

	private:
		std::filesystem::path ContentDirectory;
		std::unordered_map<std::string, std::string> ExtensionToType;
		std::unordered_map<std::string, MetaInitializer> MetaInitializers;
		std::vector<std::string> EditorOnlyExtensions;
		std::vector<std::string> EditorOnlyTypes;

		struct SubAssetType
		{
			std::string SectionType;
			std::string TypeName;
		};
		std::unordered_map<std::string, std::vector<SubAssetType>> SubAssetTypes; // by parent type

		std::vector<AssetInfo> Assets;
		std::unordered_map<Uid, size_t> AssetIndexById;

		// Which file (relative path, generic form) owned each id at the last refresh.
		std::unordered_map<Uid, std::string> PreviousOwners;
		std::filesystem::path OwnerCacheFile;
	};
}
