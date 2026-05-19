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
			folder->AddAsset(std::move(CreateAssetFromFilePath(entry.path().generic_string())));
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

	int stringsize = 0;
	inputstream.read(reinterpret_cast<char*>(&stringsize), sizeof(stringsize));

	if (stringsize <= 0)
	{
		return nullptr;
	}

	std::string assetType(stringsize, '\0');
	inputstream.read(assetType.data(), stringsize);
	if (assetType.empty()) {
		return nullptr;
	}

	auto asset = AssetRegistery::Create(assetType);
	asset->Load(inputstream);

	return std::move(asset);
}

void AssetManager::SaveAsset(std::unique_ptr<Asset>& asset)
{
	std::ofstream outputstream;
	outputstream.open(asset->GetPath(), std::ios::binary | std::ios::out);
	if (!outputstream.is_open())
	{
		printf("failed to save asset: %s", asset->GetPath().c_str());
		return;
	}

	const std::string assetType = asset->GetType();
	const size_t stringSize = assetType.size();
	outputstream.write(reinterpret_cast<const char*>(stringSize), sizeof(stringSize));
	outputstream.write(reinterpret_cast<const char*>(assetType.data()), stringSize);

	asset->Save(outputstream);

	if (!outputstream)
	{
		printf("failed while writing asset: %s", asset->GetPath().c_str());
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
	if (AssetImported(filepath))
	{
		// .asset already exists lets load that instead.
		std::string pathWithoutExtention = filepath.substr(0, filepath.find_last_of("."));
		return CreateAssetFromFilePath(pathWithoutExtention + ".asset");
	}

	// Time to create a .asset from a import (.mp3 .png .wav etc)
	std::string extention = filepath.substr(filepath.find_first_of("."));
	auto asset = AssetRegistery::CreateFromExtention(extention);
	asset->ImportFrom(filepath);

	return std::move(asset);
}


