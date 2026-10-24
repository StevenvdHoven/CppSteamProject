#include "Core/Input/InputHandlerRegistery.h"

#include <SDL3/SDL_log.h>

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace Flex
{
	namespace
	{
		struct RegisteryData
		{
			std::mutex Mutex;
			std::unordered_map<std::string, InputHandler> Handlers;
			uint64_t Revision = 1;
		};

		RegisteryData& GetData()
		{
			static RegisteryData data;
			return data;
		}
	}

	const char* ToString(InputTrigger trigger)
	{
		switch (trigger) {
		case InputTrigger::Pressed: return "Pressed";
		case InputTrigger::Released: return "Released";
		case InputTrigger::Held: return "Held";
		case InputTrigger::Always: return "Always";
		}
		return "Pressed";
	}

	bool ParseInputTrigger(std::string_view text, InputTrigger& outTrigger)
	{
		for (const InputTrigger trigger : { InputTrigger::Pressed, InputTrigger::Released, InputTrigger::Held, InputTrigger::Always }) {
			if (text == ToString(trigger)) {
				outTrigger = trigger;
				return true;
			}
		}
		return false;
	}

	void InputHandlerRegistery::Register(const char* name, InputHandler function)
	{
		RegisteryData& data = GetData();
		std::scoped_lock lock{ data.Mutex };
		const auto [iterator, bAdded] = data.Handlers.emplace(name, function);
		if (!bAdded && iterator->second != function) {
			SDL_Log("Input: two input handlers are called '%s'; the second one is ignored (handler names must be unique).", name);
			return;
		}
		data.Revision++;
	}

	InputHandler InputHandlerRegistery::Find(std::string_view name)
	{
		RegisteryData& data = GetData();
		std::scoped_lock lock{ data.Mutex };
		const auto found = data.Handlers.find(std::string(name));
		return found != data.Handlers.end() ? found->second : nullptr;
	}

	std::vector<InputHandlerInfo> InputHandlerRegistery::GetHandlers()
	{
		RegisteryData& data = GetData();
		std::vector<InputHandlerInfo> handlers;
		{
			std::scoped_lock lock{ data.Mutex };
			for (const auto& [name, function] : data.Handlers) {
				handlers.push_back({ name, function });
			}
		}
		std::sort(handlers.begin(), handlers.end(), [](const InputHandlerInfo& a, const InputHandlerInfo& b) { return a.Name < b.Name; });
		return handlers;
	}

	size_t InputHandlerRegistery::Remove(const std::function<bool(const InputHandlerInfo&)>& predicate)
	{
		RegisteryData& data = GetData();
		std::scoped_lock lock{ data.Mutex };
		const size_t removed = std::erase_if(data.Handlers, [&](const auto& entry) { return predicate(InputHandlerInfo{ entry.first, entry.second }); });
		if (removed > 0) {
			data.Revision++;
		}
		return removed;
	}

	uint64_t InputHandlerRegistery::GetRevision()
	{
		RegisteryData& data = GetData();
		std::scoped_lock lock{ data.Mutex };
		return data.Revision;
	}
}
