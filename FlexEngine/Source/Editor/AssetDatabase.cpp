#include "Editor/AssetDatabase.h"
#include "Core/FileSystem.h"
#include "Core/Serialization/TextArchive.h"
#include "Core/Serialization/TextDocument.h"
#include "Core/Serialization/TextValue.h"
#include <algorithm>

namespace Flex
{
	namespace
	{
		constexpr const char* METASECTION = "Meta";
		constexpr const char* OWNERSECTION = "AssetOwners";

		std::string ToLowerAscii(std::string text)
		{
			for (char& c : text) {
				if (c >= 'A' && c <= 'Z') {
					c = static_cast<char>(c - 'A' + 'a');
				}
			}
			return text;
		}

		std::filesystem::path MetaPathFor(const std::filesystem::path& sourceFile)
		{
			std::filesystem::path metaPath = sourceFile;
			metaPath += AssetDatabase::MetaExtension;
			return metaPath;
		}

		// One source file during a refresh, with its .meta and every id it claims.
		struct ScannedFile
		{
			std::filesystem::path SourceFile;
			std::filesystem::path RelativePath;
			std::string RelativeKey; // generic form, for comparing with the owner cache
			std::string Type;
			TextDocument Document;
			Uid Id;
			bool bNeedsSave = false;

			struct SubAsset
			{
				size_t SectionIndex;
				Uid Id;
				std::string Name;
				std::string TypeName;
			};
			std::vector<SubAsset> SubAssets;
		};

		// A place an id is used: the file itself (SubIndex == npos) or one of its sub-assets.
		struct IdClaim
		{
			size_t FileIndex;
			size_t SubIndex;
		};
		constexpr size_t NOSUB = static_cast<size_t>(-1);
	}

	void AssetDatabase::RegisterType(const std::string& typeName, const std::vector<std::string>& extensions, MetaInitializer initializeMeta, AssetTypeFlags flags)
	{
		const bool bEditorOnly = (static_cast<uint8_t>(flags) & static_cast<uint8_t>(AssetTypeFlags::EditorOnly)) != 0;
		for (const std::string& extension : extensions) {
			ExtensionToType[ToLowerAscii(extension)] = typeName;
			if (bEditorOnly) {
				EditorOnlyExtensions.push_back(ToLowerAscii(extension));
			}
		}
		if (bEditorOnly) {
			EditorOnlyTypes.push_back(typeName);
		}
		if (initializeMeta) {
			MetaInitializers[typeName] = std::move(initializeMeta);
		}
	}

	bool AssetDatabase::IsEditorOnly(const AssetInfo& asset) const
	{
		return std::find(EditorOnlyTypes.begin(), EditorOnlyTypes.end(), asset.Type) != EditorOnlyTypes.end();
	}

	void AssetDatabase::RegisterSubAssetType(const std::string& parentType, const std::string& sectionType, const std::string& subAssetType)
	{
		SubAssetTypes[parentType].push_back(SubAssetType{ sectionType, subAssetType });
	}

	void AssetDatabase::SetOwnerCacheFile(const std::filesystem::path& cacheFile)
	{
		OwnerCacheFile = cacheFile;

		std::error_code errorCode;
		if (!std::filesystem::exists(cacheFile, errorCode)) {
			return;
		}

		TextDocument document;
		std::string error;
		if (!document.Load(cacheFile, error)) {
			return; // only a cache: losing it just means "no history"
		}
		if (const TextSection* section = document.FindSection(OWNERSECTION)) {
			for (const TextProperty& property : section->Properties)
			{
				const std::optional<Uid> id = Uid::FromString(property.Key);
				std::string path;
				if (id && TextValue::ParseString(property.Value, path)) {
					PreviousOwners[*id] = path;
				}
			}
		}
	}

	void AssetDatabase::SaveOwnerCache() const
	{
		if (OwnerCacheFile.empty()) {
			return;
		}

		TextDocument document;
		TextSection& section = document.AddSection(OWNERSECTION);
		section.LeadingComments.push_back("Editor cache (per machine, not for source control): which file owned which asset id at the last refresh.");
		for (const AssetInfo& asset : Assets) {
			section.SetValue(asset.Id.ToString(), TextValue::FormatString(PathToGenericUtf8(asset.Path)));
		}

		std::string error;
		document.Save(OwnerCacheFile, error);
	}

	bool AssetDatabase::LoadMeta(const AssetInfo& asset, TextDocument& outMeta, std::string& outError) const
	{
		return outMeta.Load(MetaPathFor(GetSourcePath(asset)), outError);
	}

	bool AssetDatabase::SaveMeta(const AssetInfo& asset, const TextDocument& meta, std::string& outError) const
	{
		return meta.Save(MetaPathFor(GetSourcePath(asset)), outError);
	}

	const std::string* AssetDatabase::GetTypeForExtension(const std::filesystem::path& extension) const
	{
		const auto found = ExtensionToType.find(ToLowerAscii(PathToUtf8(extension)));
		return found != ExtensionToType.end() ? &found->second : nullptr;
	}

	AssetRefreshReport AssetDatabase::Refresh(const std::filesystem::path& contentDirectory)
	{
		AssetRefreshReport report;
		ContentDirectory = contentDirectory;

		// The current index becomes the history for this refresh (on top of the cache file).
		for (const AssetInfo& asset : Assets) {
			PreviousOwners[asset.Id] = PathToGenericUtf8(asset.Path);
		}
		Assets.clear();
		AssetIndexById.clear();

		std::error_code errorCode;
		if (!std::filesystem::is_directory(contentDirectory, errorCode)) {
			++report.Errors;
			report.Messages.push_back("Content folder not found: " + PathToUtf8(contentDirectory));
			return report;
		}

		// -----------------------
		// 1. Find files. Sorted, so the result is deterministic.
		// -----------------------
		std::vector<std::filesystem::path> sourceFiles;
		std::vector<std::filesystem::path> metaFiles;
		for (auto iterator = std::filesystem::recursive_directory_iterator(contentDirectory, std::filesystem::directory_options::skip_permission_denied, errorCode);
			iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(errorCode))
		{
			if (errorCode) {
				break;
			}
			if (!iterator->is_regular_file(errorCode)) {
				continue;
			}

			const std::filesystem::path& filePath = iterator->path();
			if (ToLowerAscii(PathToUtf8(filePath.extension())) == MetaExtension) {
				metaFiles.push_back(filePath);
			}
			else if (GetTypeForExtension(filePath.extension())) {
				sourceFiles.push_back(filePath);
			}
		}
		std::sort(sourceFiles.begin(), sourceFiles.end());
		std::sort(metaFiles.begin(), metaFiles.end());

		// -----------------------
		// 2. Read (or create) every .meta and collect the ids it claims.
		// -----------------------
		std::vector<ScannedFile> files;
		files.reserve(sourceFiles.size());
		for (const std::filesystem::path& sourceFile : sourceFiles)
		{
			ScannedFile file;
			file.SourceFile = sourceFile;
			file.RelativePath = std::filesystem::relative(sourceFile, contentDirectory, errorCode);
			file.RelativeKey = PathToGenericUtf8(file.RelativePath);
			file.Type = *GetTypeForExtension(sourceFile.extension());
			const std::filesystem::path metaPath = MetaPathFor(sourceFile);

			if (std::filesystem::exists(metaPath, errorCode))
			{
				std::string error;
				if (!file.Document.Load(metaPath, error)) {
					// Don't overwrite a file we can't read: it may hold settings the user wants back.
					++report.Errors;
					report.Messages.push_back(error + " (asset skipped, fix or delete the .meta)");
					continue;
				}

				std::string storedType;
				if (TextSection* section = file.Document.FindSection(METASECTION)) {
					TextArchive archive(*section, ArchiveMode::Load);
					archive.SerializeVersion(MetaVersion);
					archive.Field("Id", file.Id);
					archive.Field("Type", storedType);
					for (const std::string& warning : archive.GetWarnings()) {
						report.Messages.push_back(PathToUtf8(metaPath) + ": " + warning);
					}
				}

				if (!file.Id.IsValid()) {
					file.Id = Uid::Generate();
					file.bNeedsSave = true;
					report.Messages.push_back(file.RelativeKey + ": .meta had no valid id, gave it a new one.");
				}
				if (storedType != file.Type) {
					file.bNeedsSave = true;
				}
			}
			else
			{
				file.Id = Uid::Generate();
				file.bNeedsSave = true;
				++report.MetaCreated;

				// [Meta] first, then the type's default settings.
				file.Document.AddSection(METASECTION);
				if (const auto initializer = MetaInitializers.find(file.Type); initializer != MetaInitializers.end()) {
					initializer->second(file.Document);
				}
			}

			if (const auto subTypes = SubAssetTypes.find(file.Type); subTypes != SubAssetTypes.end())
			{
				for (const SubAssetType& subType : subTypes->second)
				{
					for (size_t sectionIndex{ 0 }; sectionIndex < file.Document.Sections.size(); ++sectionIndex)
					{
						TextSection& section = file.Document.Sections[sectionIndex];
						if (section.Type != subType.SectionType) {
							continue;
						}

						ScannedFile::SubAsset subAsset{ sectionIndex, Uid(), "", subType.TypeName };
						TextArchive archive(section, ArchiveMode::Load);
						archive.Field("Id", subAsset.Id);
						archive.Field("Name", subAsset.Name);

						if (!subAsset.Id.IsValid()) {
							subAsset.Id = Uid::Generate();
							file.bNeedsSave = true;
						}
						file.SubAssets.push_back(std::move(subAsset));
					}
				}
			}

			files.push_back(std::move(file));
		}

		// -----------------------
		// 3. Resolve duplicate ids: the previous owner keeps it, everyone else gets a new one.
		// -----------------------
		std::unordered_map<Uid, std::vector<IdClaim>> claims;
		for (size_t fileIndex{ 0 }; fileIndex < files.size(); ++fileIndex)
		{
			claims[files[fileIndex].Id].push_back(IdClaim{ fileIndex, NOSUB });
			for (size_t subIndex{ 0 }; subIndex < files[fileIndex].SubAssets.size(); ++subIndex) {
				claims[files[fileIndex].SubAssets[subIndex].Id].push_back(IdClaim{ fileIndex, subIndex });
			}
		}

		for (auto& [id, idClaims] : claims)
		{
			if (idClaims.size() < 2) {
				continue;
			}

			size_t keeper = 0;
			if (const auto owner = PreviousOwners.find(id); owner != PreviousOwners.end()) {
				for (size_t index{ 0 }; index < idClaims.size(); ++index) {
					if (files[idClaims[index].FileIndex].RelativeKey == owner->second) {
						keeper = index;
						break;
					}
				}
			}

			for (size_t index{ 0 }; index < idClaims.size(); ++index)
			{
				if (index == keeper) {
					continue;
				}

				ScannedFile& file = files[idClaims[index].FileIndex];
				const Uid newId = Uid::Generate();
				if (idClaims[index].SubIndex == NOSUB) {
					file.Id = newId;
				}
				else {
					file.SubAssets[idClaims[index].SubIndex].Id = newId;
				}
				file.bNeedsSave = true;
				++report.DuplicatesFixed;

				if (idClaims[index].SubIndex == NOSUB) {
					report.Messages.push_back(file.RelativeKey + " had the same id as " + files[idClaims[keeper].FileIndex].RelativeKey + " (copied file?). Gave the copy a new id.");
				}
			}
		}

		// -----------------------
		// 4. Write changed .meta files and build the index.
		// -----------------------
		for (ScannedFile& file : files)
		{
			if (file.bNeedsSave)
			{
				TextSection& metaSection = file.Document.FindOrAddSection(METASECTION);
				if (metaSection.LeadingComments.empty() && &metaSection == &file.Document.Sections.front()) {
					metaSection.LeadingComments.push_back("FlexEngine asset metadata. Keep this file next to " + PathToUtf8(file.SourceFile.filename()) + " and commit them together.");
				}
				{
					TextArchive archive(metaSection, ArchiveMode::Save);
					archive.SerializeVersion(MetaVersion);
					archive.Field("Id", file.Id);
					archive.Field("Type", file.Type);
				}
				for (ScannedFile::SubAsset& subAsset : file.SubAssets) {
					TextArchive archive(file.Document.Sections[subAsset.SectionIndex], ArchiveMode::Save);
					archive.Field("Id", subAsset.Id);
				}

				std::string error;
				if (!file.Document.Save(MetaPathFor(file.SourceFile), error)) {
					++report.Errors;
					report.Messages.push_back(error);
					continue;
				}
			}

			AssetIndexById[file.Id] = Assets.size();
			Assets.push_back(AssetInfo{ file.Id, file.Type, file.RelativePath, PathToUtf8(file.RelativePath.stem()), Uid() });
			for (ScannedFile::SubAsset& subAsset : file.SubAssets) {
				AssetIndexById[subAsset.Id] = Assets.size();
				Assets.push_back(AssetInfo{ subAsset.Id, subAsset.TypeName, file.RelativePath, subAsset.Name, file.Id });
			}
		}

		for (const std::filesystem::path& metaFile : metaFiles)
		{
			std::filesystem::path sourceFile = metaFile;
			sourceFile.replace_extension();
			if (!std::filesystem::exists(sourceFile, errorCode)) {
				++report.OrphanMetas;
				report.Messages.push_back(PathToUtf8(std::filesystem::relative(metaFile, contentDirectory, errorCode)) + " has no source file (moved or deleted without its .meta?).");
			}
		}

		// Remember the owners for next time.
		PreviousOwners.clear();
		for (const AssetInfo& asset : Assets) {
			PreviousOwners[asset.Id] = PathToGenericUtf8(asset.Path);
		}
		SaveOwnerCache();

		report.AssetCount = static_cast<int>(Assets.size());
		return report;
	}

	const AssetInfo* AssetDatabase::Find(Uid id) const
	{
		const auto found = AssetIndexById.find(id);
		return found != AssetIndexById.end() ? &Assets[found->second] : nullptr;
	}

	const AssetInfo* AssetDatabase::FindByPath(const std::filesystem::path& relativePath) const
	{
		const std::filesystem::path normalised = relativePath.lexically_normal();
		for (const AssetInfo& asset : Assets) {
			if (!asset.IsSubAsset() && asset.Path.lexically_normal() == normalised) {
				return &asset;
			}
		}
		return nullptr;
	}

	std::vector<const AssetInfo*> AssetDatabase::GetSubAssets(Uid parentId) const
	{
		std::vector<const AssetInfo*> result;
		for (const AssetInfo& asset : Assets) {
			if (asset.ParentId == parentId) {
				result.push_back(&asset);
			}
		}
		return result;
	}
}
