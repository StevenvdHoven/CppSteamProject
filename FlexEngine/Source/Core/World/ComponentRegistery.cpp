#include "Core/World/ComponentRegistery.h"
#include <SDL3/SDL.h>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace Flex
{
	namespace
	{
		struct RegisteryData
		{
			std::mutex Mutex;
			std::deque<ComponentTypeInfo> Types; // deque: references stay valid while it grows
			std::unordered_map<std::string, ComponentId> IdsByName;
		};

		RegisteryData& GetData()
		{
			static RegisteryData data;
			return data;
		}
	}

	const ComponentTypeInfo& ComponentRegistery::Register(const ComponentTypeInfo& info)
	{
		RegisteryData& data = GetData();
		std::scoped_lock lock{ data.Mutex };

		if (auto it = data.IdsByName.find(info.Name); it != data.IdsByName.end()) {
			const ComponentTypeInfo& existing = data.Types[it->second];
			if (existing.Size != info.Size || existing.Alignment != info.Alignment) {
				SDL_Log("ComponentRegistery: component '%s' registered twice with different layouts (size %u/%u, alignment %u/%u).",
					info.Name.c_str(), existing.Size, info.Size, existing.Alignment, info.Alignment);
				FLEX_ASSERT(false, "Two different component types use the same name.");
			}
			return existing;
		}

		ComponentTypeInfo& added = data.Types.emplace_back(info);
		added.Id = static_cast<ComponentId>(data.Types.size() - 1);
		data.IdsByName.emplace(added.Name, added.Id);
		return added;
	}

	const ComponentTypeInfo& ComponentRegistery::GetInfo(ComponentId id)
	{
		RegisteryData& data = GetData();
		std::scoped_lock lock{ data.Mutex };
		FLEX_ASSERT(id < data.Types.size(), "Unknown component id.");
		return data.Types[id];
	}

	const ComponentTypeInfo* ComponentRegistery::FindByName(const std::string& name)
	{
		RegisteryData& data = GetData();
		std::scoped_lock lock{ data.Mutex };
		auto it = data.IdsByName.find(name);
		return it != data.IdsByName.end() ? &data.Types[it->second] : nullptr;
	}

	uint32_t ComponentRegistery::GetCount()
	{
		RegisteryData& data = GetData();
		std::scoped_lock lock{ data.Mutex };
		return static_cast<uint32_t>(data.Types.size());
	}
}
