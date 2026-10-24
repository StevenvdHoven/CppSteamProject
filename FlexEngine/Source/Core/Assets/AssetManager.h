#pragma once
#include "Core/Assets/Asset.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Flex
{
	// Knows where the data for an asset Uid lives and how to load it.
	// The editor plugs in one that reads source files + .meta; exported games will
	// plug in one that reads cooked data.
	class FLEX_API AssetSource
	{
	public:
		virtual ~AssetSource() = default;

		// Returns a new, fully loaded asset, or nullptr with outError filled in.
		virtual std::shared_ptr<Asset> LoadAsset(Uid id, std::string& outName, std::string& outError) = 0;

		// The sub-assets of an asset (e.g. a texture's sprites), in their stored order.
		virtual std::vector<Uid> GetSubAssets(Uid parentId) { (void)parentId; return {}; }
	};

	// Loads assets by Uid on first use and shares them.
	//  - Asking twice for the same Uid returns the SAME object (a texture is never loaded twice).
	//  - The manager only keeps weak references: when the last user releases an asset, it's freed.
	class FLEX_API AssetManager
	{
	public:
		static void SetSource(AssetSource* source);

		template<typename T>
		static std::shared_ptr<T> Load(Uid id)
		{
			return std::dynamic_pointer_cast<T>(LoadAsset(id));
		}

		static std::shared_ptr<Asset> LoadAsset(Uid id);

		// The sub-assets of an asset (e.g. a texture's sprites), as the current source knows them.
		static std::vector<Uid> GetSubAssets(Uid parentId);

		// Already loaded? Doesn't load anything.
		static std::shared_ptr<Asset> FindLoaded(Uid id);

		// Every asset that's alive right now (for statistics/debug views).
		static std::vector<std::shared_ptr<Asset>> GetLoadedAssets();

		// Assets that failed to load aren't retried until this is called (e.g. after the files changed).
		static void ClearFailures();

		// Drops all bookkeeping. Assets that are still held elsewhere stay alive until released.
		static void Shutdown();

		// Asset types that are loaded straight from their file (no import step), the same way in the editor
		// and in games: both asset sources fall back to these for types they don't handle themselves.
		// Register in the module that defines the type (e.g. at static init); engine code only, the loader
		// must stay loaded for the whole run.
		using FileLoader = std::shared_ptr<Asset> (*)(const std::filesystem::path& file, std::string& outError);
		static void RegisterFileLoader(const std::string& typeName, FileLoader loader);
		static bool HasFileLoader(const std::string& typeName);
		// nullptr with outError when the type has no loader or loading failed.
		static std::shared_ptr<Asset> LoadFromFile(const std::string& typeName, const std::filesystem::path& file, std::string& outError);
	};
}
