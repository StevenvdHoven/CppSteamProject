#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <set>

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

	static void UpdateFolder(AssetFolder* folder);
	static std::unique_ptr<Asset> CreateAssetFromFilePath(std::string filepath);
	static void SaveAsset(std::unique_ptr<Asset>& asset);

	static AssetFolder* GetRootFolder();

	static bool AssetImported(std::string filepath);

private:
	static std::unique_ptr<Asset> CreateAssetFromImport(std::string filepath);

private:
	static bool bIntialised;
	static AssetFolder* ContentFolder;
};

