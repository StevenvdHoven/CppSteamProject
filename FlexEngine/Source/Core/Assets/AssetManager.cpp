#include "Core/Assets/AssetManager.h"
#include <SDL3/SDL_log.h>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace Flex
{
	namespace
	{
		// Recursive: loading one asset may load others (a sprite loads its texture).
		std::recursive_mutex Lock;
		AssetSource* Source = nullptr;
		std::unordered_map<Uid, std::weak_ptr<Asset>> LoadedAssets;

		// Uids that failed once; not retried every frame. Cleared when the source changes.
		std::unordered_set<Uid> FailedAssets;
	}

	void AssetManager::SetSource(AssetSource* source)
	{
		std::lock_guard<std::recursive_mutex> guard(Lock);
		Source = source;
		FailedAssets.clear();
	}

	std::shared_ptr<Asset> AssetManager::LoadAsset(Uid id)
	{
		if (!id.IsValid()) {
			return nullptr;
		}

		std::lock_guard<std::recursive_mutex> guard(Lock);

		if (const auto found = LoadedAssets.find(id); found != LoadedAssets.end()) {
			if (std::shared_ptr<Asset> asset = found->second.lock()) {
				return asset;
			}
			LoadedAssets.erase(found); // expired: was freed, load again below
		}

		if (!Source || FailedAssets.contains(id)) {
			return nullptr;
		}

		std::string name;
		std::string error;
		std::shared_ptr<Asset> asset = Source->LoadAsset(id, name, error);
		if (!asset) {
			SDL_Log("AssetManager: could not load %s: %s", id.ToString().c_str(), error.c_str());
			FailedAssets.insert(id);
			return nullptr;
		}

		asset->Id = id;
		asset->Name = name;
		LoadedAssets[id] = asset;
		return asset;
	}

	std::shared_ptr<Asset> AssetManager::FindLoaded(Uid id)
	{
		std::lock_guard<std::recursive_mutex> guard(Lock);
		const auto found = LoadedAssets.find(id);
		return found != LoadedAssets.end() ? found->second.lock() : nullptr;
	}

	std::vector<std::shared_ptr<Asset>> AssetManager::GetLoadedAssets()
	{
		std::lock_guard<std::recursive_mutex> guard(Lock);
		std::vector<std::shared_ptr<Asset>> result;
		for (auto iterator = LoadedAssets.begin(); iterator != LoadedAssets.end();)
		{
			if (std::shared_ptr<Asset> asset = iterator->second.lock()) {
				result.push_back(std::move(asset));
				++iterator;
			}
			else {
				iterator = LoadedAssets.erase(iterator);
			}
		}
		return result;
	}

	void AssetManager::ClearFailures()
	{
		std::lock_guard<std::recursive_mutex> guard(Lock);
		FailedAssets.clear();
	}

	void AssetManager::Shutdown()
	{
		std::lock_guard<std::recursive_mutex> guard(Lock);
		LoadedAssets.clear();
		FailedAssets.clear();
		Source = nullptr;
	}
}
