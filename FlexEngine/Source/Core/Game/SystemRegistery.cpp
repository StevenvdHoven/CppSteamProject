#include "Core/Game/SystemRegistery.h"
#include <algorithm>
#include <mutex>

namespace Flex
{
	namespace
	{
		struct RegisteryData
		{
			std::mutex Mutex;
			std::vector<SystemInfo> Systems;
		};

		RegisteryData& GetData()
		{
			static RegisteryData data;
			return data;
		}
	}

	void SystemRegistery::Register(SystemPhase phase, const char* name, SystemFunction function, int order)
	{
		RegisteryData& data = GetData();
		std::scoped_lock lock{ data.Mutex };
		data.Systems.push_back({ name, phase, function, order });
	}

	size_t SystemRegistery::Remove(const std::function<bool(const SystemInfo&)>& predicate)
	{
		RegisteryData& data = GetData();
		std::scoped_lock lock{ data.Mutex };
		const size_t before = data.Systems.size();
		std::erase_if(data.Systems, predicate);
		return before - data.Systems.size();
	}

	std::vector<SystemInfo> SystemRegistery::GetSystems(SystemPhase phase)
	{
		RegisteryData& data = GetData();
		std::scoped_lock lock{ data.Mutex };
		std::vector<SystemInfo> systems;
		for (const SystemInfo& system : data.Systems) {
			if (system.Phase == phase) {
				systems.push_back(system);
			}
		}
		std::sort(systems.begin(), systems.end(), [](const SystemInfo& a, const SystemInfo& b) {
			return a.Order != b.Order ? a.Order < b.Order : a.Name < b.Name;
		});
		return systems;
	}
}
