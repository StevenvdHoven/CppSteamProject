#include "Plugins/Steam/Runtime/PacketRegistery.h"
#include "Core/Game/GameModule.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace Flex::Steam
{
	namespace
	{
		struct RegisteryData
		{
			std::shared_mutex Mutex;
			std::vector<std::unique_ptr<PacketTypeInfo>> AllTypes; // stale ones too: pointers stay valid
			std::unordered_map<PacketTypeId, PacketTypeInfo*> LiveTypes;
		};

		RegisteryData& GetData()
		{
			static RegisteryData data;
			return data;
		}

		bool IsInGameModule(const PacketTypeInfo& type)
		{
			for (const void* function : { reinterpret_cast<const void*>(type.Create), reinterpret_cast<const void*>(type.Clone),
				reinterpret_cast<const void*>(type.Destroy), reinterpret_cast<const void*>(type.Write), reinterpret_cast<const void*>(type.Read),
				reinterpret_cast<const void*>(type.Receive), reinterpret_cast<const void*>(type.ReceiveMain) }) {
				if (GameModule::IsInLoadedModule(function)) {
					return true;
				}
			}
			return false;
		}

		// The game module's packet types go stale before it unloads (hot reload).
		struct GameModuleListener
		{
			uint32_t Id = 0;

			GameModuleListener()
			{
				Id = GameModule::AddListener({ []() {
					if (const size_t count = PacketRegistery::MarkStale(IsInGameModule)) {
						SDL_Log("Steam: %zu packet type(s) of the game module unregistered.", count);
					}
				}, {} });
			}

			~GameModuleListener()
			{
				GameModule::RemoveListener(Id);
			}
		};
		GameModuleListener TheGameModuleListener;
	}

	const PacketTypeInfo* PacketRegistery::Register(const PacketTypeInfo& info)
	{
		RegisteryData& data = GetData();
		std::unique_lock lock{ data.Mutex };

		if (const auto found = data.LiveTypes.find(info.Id); found != data.LiveTypes.end()) {
			PacketTypeInfo* existing = found->second;
			FLEX_ASSERT(existing->Name == info.Name, "Two packet types have the same id (hash of the name): rename one of them.");
			FLEX_ASSERT(existing->Size == info.Size && existing->Alignment == info.Alignment,
				"A packet type with this name is already registered with another size: packet names must be unique.");
			return existing;
		}
		data.AllTypes.push_back(std::make_unique<PacketTypeInfo>(info));
		PacketTypeInfo* added = data.AllTypes.back().get();
		data.LiveTypes.emplace(added->Id, added);
		return added;
	}

	const PacketTypeInfo* PacketRegistery::Find(PacketTypeId id)
	{
		RegisteryData& data = GetData();
		std::shared_lock lock{ data.Mutex };
		const auto found = data.LiveTypes.find(id);
		return found != data.LiveTypes.end() ? found->second : nullptr;
	}

	const PacketTypeInfo* PacketRegistery::FindByName(std::string_view name)
	{
		const PacketTypeInfo* type = Find(HashPacketName(name));
		return type && type->Name == name ? type : nullptr;
	}

	std::vector<const PacketTypeInfo*> PacketRegistery::GetTypes()
	{
		RegisteryData& data = GetData();
		std::vector<const PacketTypeInfo*> types;
		{
			std::shared_lock lock{ data.Mutex };
			types.reserve(data.LiveTypes.size());
			for (const auto& [id, type] : data.LiveTypes) {
				types.push_back(type);
			}
		}
		std::sort(types.begin(), types.end(), [](const PacketTypeInfo* a, const PacketTypeInfo* b) { return a->Name < b->Name; });
		return types;
	}

	uint64_t PacketRegistery::GetProtocolHash()
	{
		// FNV-1a, 64 bits, over "Name:Version;" of every type in name order.
		uint64_t hash = 14695981039346656037ull;
		const auto add = [&hash](std::string_view text) {
			for (const char character : text) {
				hash = (hash ^ static_cast<uint8_t>(character)) * 1099511628211ull;
			}
		};
		for (const PacketTypeInfo* type : GetTypes()) {
			add(type->Name);
			add(":");
			add(std::to_string(type->Version));
			add(";");
		}
		return hash;
	}

	size_t PacketRegistery::MarkStale(const std::function<bool(const PacketTypeInfo&)>& predicate)
	{
		RegisteryData& data = GetData();
		std::unique_lock lock{ data.Mutex };
		size_t count = 0;
		for (auto iterator = data.LiveTypes.begin(); iterator != data.LiveTypes.end();) {
			if (predicate(*iterator->second)) {
				iterator->second->bIsStale = true;
				iterator = data.LiveTypes.erase(iterator);
				++count;
			} else {
				++iterator;
			}
		}
		return count;
	}
}
