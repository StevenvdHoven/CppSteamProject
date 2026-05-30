#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <set>
#include <type_traits>
#include "Asset.h"
#include <imgui/imgui.h>
#include "BinarySaving.h"

class Asset;

class AssetFolder 
{
public:
	AssetFolder(std::string folderPath, bool force = false);

	void AddFolder(std::unique_ptr<AssetFolder>&& folder);
	void RemoveFolder(AssetFolder* folder);
	void AddAsset(std::unique_ptr<Asset>&& asset);
	void RemoveAsset(Asset* asset);

	const std::set<std::unique_ptr<AssetFolder>>& GetFolders() const { return Folders;  }
	const std::set<std::unique_ptr<Asset>>& GetAssets() const { return Assets; }

	const std::string& GetName() const { return Name; }
	const std::string& GetPath() const { return FolderPath; }

private:
	std::string Name;
	std::string FolderPath;
	std::set<std::unique_ptr<AssetFolder>> Folders;
	std::set<std::unique_ptr<Asset>> Assets;
};
class AssetManager
{
public:
	static void Init();
	static void CleanUp();

	static void UpdateFolder(AssetFolder* folder, bool assetOnly = false);
	static std::unique_ptr<Asset> CreateAssetFromFilePath(std::string filepath);
	static void SaveAsset(Asset* asset, bool dup = false, bool rename = false);

	static AssetFolder* FindFolder(std::string path);
	static AssetFolder* FindFolder(AssetFolder* parentFolder, std::string path);
	static Asset* FindAsset(const std::string& filepath);
	static Asset* FindAsset(AssetFolder* parentFolder, const std::string& filepath);
	static std::vector<Asset*> GetAssets(std::set<std::string> filter = std::set<std::string>());
	static void GetAssets(AssetFolder* parentFolder, std::vector<Asset*>& foundassets, std::set<std::string> filter = std::set<std::string>());
	static AssetFolder* GetRootFolder();

	static bool AssetImported(std::string filepath);

private:
	static std::unique_ptr<Asset> CreateAssetFromImport(std::string filepath);

private:
	static bool bIntialised;
	static AssetFolder* ContentFolder;
};

template<typename AssetType>
	requires std::is_base_of_v<Asset, AssetType>
class AssetProperty
{
	static_assert(std::is_base_of_v<Asset, AssetType>,
		"AssetType must derive from Asset");
public:

	AssetType* Get() { return AssetRef; }
	void Set(AssetType* newRef) { AssetRef = newRef; }


	void Load(std::ifstream& inputstream) {

		std::string path = BinarySaving::ReadString(inputstream);
		if (!path.empty())
		{
			if (AssetType* foundAsset = dynamic_cast<AssetType*>(AssetManager::FindAsset(path))) {
				AssetRef = foundAsset;
			}
			else {
				printf("failed to load asset in AssetProperty: %s", path.c_str());
			}
		}
	}

	void Save(std::ofstream& outputstream) {
		Asset* asset = AssetRef;
		if (asset) {
			BinarySaving::Write(outputstream, asset->GetPath());
		}
		else {
			BinarySaving::Write(outputstream, "");
		}
	}

	void ImGui_OnDrawProperty(const char* label) {
		AssetType temp{};
		Asset* tempPtr = (Asset*)&temp;

		std::set<std::string> filter;
		filter.insert(tempPtr->GetType());

		std::vector<Asset*> foundAssets = AssetManager::GetAssets(filter);

		std::string selectedAsset = "None";
		if (AssetRef) {
			Asset* assetPtr = (Asset*)AssetRef;
			selectedAsset = assetPtr->GetName();
		}
		
		if (ImGui::BeginCombo(label, selectedAsset.c_str()))
		{
			if (ImGui::Selectable("None")) {
				AssetRef = nullptr;
			}
			for (int n = 0; n < foundAssets.size(); n++)
			{
				if (ImGui::Selectable(foundAssets[n]->GetName().c_str())) {
					AssetRef = dynamic_cast<AssetType*>(foundAssets[n]);
				}
			}
			ImGui::EndCombo();
		}
	}

	AssetType* operator->() const { return AssetRef; }
private:
	AssetType* AssetRef = nullptr;
};