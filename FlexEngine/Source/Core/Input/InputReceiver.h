#pragma once
#include "Core/Core.h"
#include "Core/Input/InputHandlerRegistery.h"
#include "Core/Input/InputMap.h"
#include "Core/Serialization/Archive.h"
#include "Core/Uid.h"
#include "Core/World/ComponentRegistery.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Flex
{
	// One handler on one action of the map: "when Jump is Pressed, call OnJump".
	struct InputHandlerBinding
	{
		static constexpr uint32_t Version = 1;

		std::string Action;
		InputTrigger Trigger = InputTrigger::Pressed;
		std::string Handler; // the name of a FLEX_INPUT_HANDLER function (kept even when it doesn't exist)

		FLEX_API void Serialize(Archive& archive);
	};

	// Gives an entity an InputMap and a player: every frame the DispatchInput system (Update, order -900,
	// before the game's systems) calls the handlers bound to its actions. Bind them in the Inspector, or
	// read actions from code with Input::GetAction(world, entity, "Move").
	struct InputReceiver
	{
		FLEX_COMPONENT(InputReceiver)
		static constexpr uint32_t Version = 1;

		Uid Map;                // an InputMap asset
		int32_t PlayerIndex = 0; // 0 = keyboard + mouse + first gamepad; 1.. = the next gamepads
		std::vector<InputHandlerBinding> Bindings;

		// Runtime cache (not saved): the loaded map.
		std::shared_ptr<InputMap> Loaded;
		Uid LoadedId;

		FLEX_API void Serialize(Archive& archive);

		// The map, loaded on first use (nullptr when Map is empty or doesn't load).
		FLEX_API const InputMap* GetMap();
	};
}
