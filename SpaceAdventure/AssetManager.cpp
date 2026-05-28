#include "AssetManager.h"
#include "Asset.h"
#include <iostream>
#include <filesystem>
#include <windows.h>
#include <commdlg.h>

#pragma comment(lib, "Comdlg32.lib")

std::string SaveFileDialog(const char* defaultName = "NewAsset.asset")
{
	char filename[MAX_PATH] = "";

	strcpy_s(filename, defaultName);

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;

	ofn.lpstrFilter =
		"Asset Files\0*.asset\0"
		"All Files\0*.*\0";

	ofn.nFilterIndex = 1;

	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

	ofn.lpstrDefExt = "asset";

	if (GetSaveFileNameA(&ofn))
	{
		return filename;
	}

	return "";
}

AssetFolder::AssetFolder(std::string folderPath, bool force):
	FolderPath{folderPath}
{
#ifdef _DEBUG
	std::cout << "Creating Asset folder: " << folderPath << std::endl;
#endif // _DEBUG

	if (!std::filesystem::exists(folderPath) && force)
	{
		std::filesystem::create_directory(folderPath);
	}

	Name = folderPath.substr(folderPath.find_last_of("/\\") + 1);
	if (Name.empty())
	{
		Name = folderPath.substr(0, folderPath.find_first_of("/"));
	}
	AssetManager::UpdateFolder(this);
}

void AssetFolder::AddFolder(std::unique_ptr<AssetFolder>&& folder)
{
	auto foundit = std::find_if(Folders.begin(), Folders.end(), [&](const std::unique_ptr<AssetFolder>& checkFolder) {
		return folder->GetPath() == checkFolder->GetPath();
	});

	if (foundit == Folders.end()) {
		Folders.emplace(std::move(folder));
	}
	else {
		// handle duplicate folder
	}
}

void AssetFolder::RemoveFolder(AssetFolder* folder)
{
}

void AssetFolder::AddAsset(std::unique_ptr<Asset>&& asset)
{
	if (asset == nullptr) {
		return;
	}

	auto foundit = std::find_if(Assets.begin(), Assets.end(), [&](const std::unique_ptr<Asset>& checkAsset)
		{
			return asset->GetPath() == checkAsset->GetPath();
		});

	if (foundit == Assets.end())
	{
		Assets.emplace(std::move(asset));
	}
}

void AssetFolder::RemoveAsset(Asset* asset)
{
	auto foundit = std::find_if(Assets.begin(), Assets.end(), [&](const std::unique_ptr<Asset>& checkAsset)
		{
			return asset == checkAsset.get();
		});

	if (foundit == Assets.end())
	{
		Assets.erase(foundit);
	}
}

AssetFolder* AssetManager::ContentFolder = nullptr;
bool AssetManager::bIntialised = false;

void AssetManager::Init()
{
	if (bIntialised)
	{
		return;
	}

	ContentFolder = new AssetFolder("Content/", true);
	bIntialised = true;
}

void AssetManager::CleanUp()
{
	delete ContentFolder;
	bIntialised = false;
}

void AssetManager::UpdateFolder(AssetFolder* folder, bool assetOnly)
{
	const std::string& path{ folder->GetPath() };
#ifdef _DEBUG
	std::cout << "Updating folder: " << path << std::endl;
#endif
	for (const auto& entry : std::filesystem::directory_iterator(path))
	{
		if (!entry.is_directory()) {
			folder->AddAsset(std::move(CreateAssetFromFilePath(entry.path().generic_string())));
		}
		else if(!assetOnly) {
#ifdef _DEBUG
			std::cout << "Found folder: " << entry.path().generic_string() << std::endl;
#endif
			folder->AddFolder(std::move(std::make_unique<AssetFolder>(entry.path().generic_string())));

		}
	}
#ifdef _DEBUG
	std::cout << "Done updating folder: " << path << std::endl;
#endif

}

std::unique_ptr<Asset> AssetManager::CreateAssetFromFilePath(std::string filepath)
{
	auto pos = filepath.find_last_of(".");
	if (pos == std::string::npos) {
		return nullptr;
	}

	std::string extention = filepath.substr(pos);
	if (extention.empty()) {
		return nullptr;
	}

	if (extention != ".asset")
	{
		return CreateAssetFromImport(filepath);
	}

	std::ifstream inputstream;
	inputstream.open(filepath, std::ios::binary | std::ios::in);
	if (!inputstream.is_open() || inputstream.bad())
	{
		return nullptr;
	}

	std::string assetType;
	std::string source;
	BinarySaving::ReadAsset(inputstream, assetType, source);
	if (assetType.empty()) {
		return nullptr;
	}

	auto asset = AssetRegistery::Create(assetType);
	asset->Source = source;
	asset->FilePath = filepath;

	auto start = filepath.find_last_of("/") + 1;
	auto end = filepath.find_first_of(".");
	asset->Name = filepath.substr(start, end - start);

	asset->Load(inputstream);

	return std::move(asset);
}

void AssetManager::SaveAsset(Asset* asset, bool dup, bool rename)
{
	if (asset->FilePath.empty())
	{
		asset->FilePath = SaveFileDialog((asset->Name + ".asset").c_str());

		if (asset->FilePath.empty())
			return;
	}

	if (rename)
	{
		std::string newpath = asset->FilePath.substr(0 ,asset->FilePath.find_last_of("/") + 1) + asset->Name + ".asset";
		if (asset->Source.substr(0, asset->Source.find_last_of(".")) == asset->FilePath.substr(0, asset->FilePath.find_last_of("."))) {
			std::string newsourcepath = asset->Source.substr(asset->Source.find_last_of("."));
			newsourcepath = newpath.substr(0, newpath.find_last_of(".")) + newsourcepath;
			std::filesystem::rename(asset->Source, newsourcepath);
			asset->Source = newsourcepath;
		}
		std::filesystem::rename(asset->FilePath, newpath);
		asset->FilePath = newpath;
	}

	if (dup)
	{
		std::string pathWithoutExtention = asset->FilePath.substr(0, asset->FilePath.find_first_of("."));
		asset->FilePath = pathWithoutExtention + "_dup";
		while (std::filesystem::exists(asset->FilePath + ".asset"))
		{
			asset->FilePath += "_dup";
		}
		asset->FilePath += ".asset";
	}

	std::ofstream outputstream;
	outputstream.open(asset->GetPath(), std::ios::binary | std::ios::out);
	if (!outputstream.is_open())
	{
		printf("failed to save asset: %s", asset->GetPath().c_str());
		return;
	}

	const std::string assetType = asset->GetType();
	std::uint32_t stringSize = static_cast<std::uint32_t>(assetType.size());
	outputstream.write(reinterpret_cast<const char*>(&stringSize), sizeof(stringSize));
	outputstream.write(reinterpret_cast<const char*>(assetType.data()), stringSize);

	std::uint32_t sourceSize = static_cast<std::uint32_t>(asset->Source.size());
	outputstream.write(reinterpret_cast<const char*>(&sourceSize), sizeof(sourceSize));
	outputstream.write(reinterpret_cast<const char*>(asset->Source.data()), sourceSize);

	asset->Save(outputstream);

	if (!outputstream)
	{
		printf("failed while writing asset: %s", asset->GetPath().c_str());
	}

	outputstream.close();

	if (dup || rename)
	{
		std::string folderPath = asset->GetPath().substr(0, asset->FilePath.find_last_of("/"));
		UpdateFolder(FindFolder(folderPath));
	}
}

AssetFolder* AssetManager::FindFolder(std::string path)
{
	return FindFolder(ContentFolder, path);
}

AssetFolder* AssetManager::FindFolder(AssetFolder* parentFolder, std::string path)
{
	if (parentFolder->GetPath() == path) {
		return parentFolder;
	}

	for (const auto& folder : parentFolder->GetFolders())
	{
		if (AssetFolder* found = FindFolder(folder.get(), path)) {
			return found;
		}
	}
	return nullptr;
}

Asset* AssetManager::FindAsset(const std::string& filepath)
{
	return FindAsset(ContentFolder, filepath);
}

Asset* AssetManager::FindAsset(AssetFolder* parentFolder, const std::string& filepath)
{
	for (const auto& asset : parentFolder->GetAssets())
	{
		if (asset->GetPath() == filepath) {
			return asset.get();
		}
	}

	for (const auto& folder : parentFolder->GetFolders())
	{
		if (Asset* found = FindAsset(folder.get(), filepath)) {
			return found;
		}
	}
	return nullptr;
}

std::vector<Asset*> AssetManager::GetAssets(std::set<std::string> filter)
{
	std::vector<Asset*> foundAssets;
	GetAssets(ContentFolder, foundAssets, filter);

	std::sort(foundAssets.begin(), foundAssets.end(), [](Asset* first, Asset* second) {
		return first < second;
	});

	return foundAssets;
}

void AssetManager::GetAssets(AssetFolder* parentFolder, std::vector<Asset*>& foundassets, std::set<std::string> filter)
{
	for (const auto& asset : parentFolder->GetAssets())
	{
		if (filter.size() && !filter.contains(asset->GetType())) {
			continue;
		}
		foundassets.emplace_back(asset.get());
	}

	for (const auto& folder : parentFolder->GetFolders())
	{
		GetAssets(folder.get(), foundassets, filter);
	}
}

AssetFolder* AssetManager::GetRootFolder()
{
	if (!bIntialised) {
		return nullptr;
	}

	return ContentFolder;
}

bool AssetManager::AssetImported(std::string filepath)
{
	auto pos = filepath.find_last_of(".");
	if (pos == std::string::npos) {
		return false;
	}

	std::string pathWithoutExtention = filepath.substr(0, pos);
	return std::filesystem::exists(filepath + ".asset");
}

std::unique_ptr<Asset> AssetManager::CreateAssetFromImport(std::string filepath)
{
	std::string pathWithoutExtention = filepath.substr(0, filepath.find_last_of("."));
	if (AssetImported(filepath))
	{
		// .asset already exists lets load that instead.
		return CreateAssetFromFilePath(pathWithoutExtention + ".asset");
	}

	// Time to create a .asset from a import (.mp3 .png .wav etc)
	std::string extention = filepath.substr(filepath.find_first_of("."));
	auto asset = AssetRegistery::CreateFromExtention(extention);
	if (asset == nullptr) {
		return nullptr;
	}

	asset->ImportFrom(filepath);
	asset->FilePath = pathWithoutExtention + ".asset";
	asset->Source = filepath;
	asset->Name = filepath.substr(filepath.find_last_of("/"));

	SaveAsset(asset.get());
	return std::move(asset);
}


