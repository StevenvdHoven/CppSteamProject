#pragma once
#include "Core/Core.h"
#include "Core/Input/InputMap.h"
#include "Core/World/Entity.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Flex
{
	class World;

	// When an InputReceiver calls a handler for an action.
	enum class InputTrigger : uint8_t
	{
		Pressed,  // the frame the action became active (buttons: pressed; axes: moved away from 0)
		Released, // the frame it stopped being active
		Held,     // every frame while it's active
		Always    // every frame, also at 0 (e.g. "Move", so the handler sees it stop)
	};

	FLEX_API const char* ToString(InputTrigger trigger);
	FLEX_API bool ParseInputTrigger(std::string_view text, InputTrigger& outTrigger);

	// What a handler gets.
	struct InputEvent
	{
		std::string_view Action;
		InputTrigger Trigger = InputTrigger::Pressed;
		InputActionState State; // Value, bIsActive, bPressed, bReleased
		float DeltaTime = 0.0f; // the frame time (Held/Always run once per frame)
		uint32_t Player = 0;

		Vector2 GetVector() const { return State.Value; }
		float GetAxis() const { return State.Value.x; }
	};

	// An input handler is a plain function in game code:
	//
	//     void OnJump(World& world, Entity entity, const InputEvent& event) { ... }
	//     FLEX_INPUT_HANDLER(OnJump)
	//
	// It's picked by name in the Inspector of an InputReceiver (per action, with a trigger) and saved in
	// the scene by that name. It runs in the Update phase, outside World::Each, so it may spawn,
	// destroy, add and remove components (applied after the system, like any system's commands).
	using InputHandler = void (*)(World& world, Entity entity, const InputEvent& event);

	struct InputHandlerInfo
	{
		std::string Name;
		InputHandler Function = nullptr;
	};

	// Process-wide list of input handlers, filled by FLEX_INPUT_HANDLER when a module loads. The game
	// module's handlers are removed when it unloads (hot reload) and come back with the rebuilt module.
	class FLEX_API InputHandlerRegistery
	{
	public:
		// A second function with the same name is refused (logged): handler names are unique in a game.
		static void Register(const char* name, InputHandler function);
		static InputHandler Find(std::string_view name); // nullptr when unknown
		static std::vector<InputHandlerInfo> GetHandlers(); // sorted by name
		static size_t Remove(const std::function<bool(const InputHandlerInfo&)>& predicate);
		// Changes whenever a handler is added or removed (so callers can cache lookups).
		static uint64_t GetRevision();
	};

	// Registers an input handler when its module loads (put it next to the function).
	#define FLEX_INPUT_HANDLER(Function) \
		static const bool FlexInputHandlerRegistered_##Function = (::Flex::InputHandlerRegistery::Register(#Function, &Function), true);
}
