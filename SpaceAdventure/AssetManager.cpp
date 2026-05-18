#include "AssetManager.h"
#include "Asset.h"
#include <iostream>
#include <filesystem>



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

void AssetManager::UpdateFolder(AssetFolder* folder)
{
	const std::string& path{ folder->GetPath() };
#ifdef _DEBUG
	std::cout << "Updating folder: " << path << std::endl;
#endif
	for (const auto& entry : std::filesystem::directory_iterator(path))
	{
		if (entry.is_directory()) {
#ifdef _DEBUG
			std::cout << "Found folder: " << entry.path().generic_string() << std::endl;
#endif
			folder->AddFolder(std::move(std::make_unique<AssetFolder>(entry.path().generic_string())));
			
		}
		else {
			// Add Asset;
		}
	}
#ifdef _DEBUG
	std::cout << "Done updating folder: " << path << std::endl;
#endif

}

AssetFolder* AssetManager::GetRootFolder()
{
	if (!bIntialised) {
		return nullptr;
	}

	return ContentFolder;
}


