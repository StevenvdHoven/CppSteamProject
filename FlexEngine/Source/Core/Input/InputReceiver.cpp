#include "Core/Input/InputReceiver.h"
#include "Core/Assets/AssetManager.h"
#include "Core/Game/SystemRegistery.h"
#include "Core/Input/Input.h"
#include "Core/World/World.h"

#include <algorithm>

namespace Flex
{
	FLEX_REGISTER_COMPONENT(InputReceiver)

	namespace
	{
		const InputActionState EmptyState;

		bool ShouldFire(InputTrigger trigger, const InputActionState& state)
		{
			switch (trigger) {
			case InputTrigger::Pressed: return state.bPressed;
			case InputTrigger::Released: return state.bReleased;
			case InputTrigger::Held: return state.bIsActive;
			case InputTrigger::Always: return true;
			}
			return false;
		}

		struct PendingCall
		{
			Entity Target;
			InputHandler Function = nullptr;
			std::string Action;
			InputEvent Event;
		};

		// Update, before the game's systems: calls every InputReceiver's handlers whose trigger happened.
		// The calls are collected first and made outside World::Each, so handlers may change the world.
		void DispatchInput(World& world, float deltaTime)
		{
			std::vector<PendingCall> calls;
			world.Each<InputReceiver>([&](Entity entity, InputReceiver& receiver) {
				const InputMap* map = receiver.GetMap();
				if (!map) {
					return;
				}
				const uint32_t player = static_cast<uint32_t>(std::clamp(receiver.PlayerIndex, 0, static_cast<int32_t>(Input::MaxPlayers) - 1));
				for (const InputHandlerBinding& binding : receiver.Bindings) {
					const int action = map->FindAction(binding.Action);
					if (action < 0) {
						continue;
					}
					const InputActionState& state = map->GetState(action, player);
					if (!ShouldFire(binding.Trigger, state)) {
						continue;
					}
					const InputHandler function = InputHandlerRegistery::Find(binding.Handler);
					if (!function) {
						continue;
					}
					PendingCall call;
					call.Target = entity;
					call.Function = function;
					call.Action = binding.Action;
					call.Event.Trigger = binding.Trigger;
					call.Event.State = state;
					call.Event.DeltaTime = deltaTime;
					call.Event.Player = player;
					calls.push_back(std::move(call));
				}
			});
			for (PendingCall& call : calls) {
				call.Event.Action = call.Action;
				if (world.IsAlive(call.Target)) {
					call.Function(world, call.Target, call.Event);
				}
			}
		}
	}
	FLEX_REGISTER_SYSTEM_ORDER(Update, DispatchInput, -900)

	void InputHandlerBinding::Serialize(Archive& archive)
	{
		archive.SerializeVersion(Version);
		std::string trigger = ToString(Trigger);
		archive.Field("Action", Action);
		archive.Field("Trigger", trigger);
		archive.Field("Handler", Handler);
		if (archive.IsLoading()) {
			ParseInputTrigger(trigger, Trigger);
		}
	}

	void InputReceiver::Serialize(Archive& archive)
	{
		archive.SerializeVersion(Version);
		archive.Field("Map", Map, { InputMap::TypeName });
		archive.Field("Player", PlayerIndex);
		archive.ArrayField("Bindings", Bindings);
	}

	const InputMap* InputReceiver::GetMap()
	{
		if (LoadedId != Map) {
			Loaded = Map.IsValid() ? AssetManager::Load<InputMap>(Map) : nullptr;
			LoadedId = Map;
		}
		return Loaded.get();
	}

	const InputActionState& Input::GetAction(World& world, Entity entity, std::string_view action)
	{
		InputReceiver* receiver = world.GetComponent<InputReceiver>(entity);
		const InputMap* map = receiver ? receiver->GetMap() : nullptr;
		if (!map) {
			return EmptyState;
		}
		return map->GetState(action, static_cast<uint32_t>(std::clamp(receiver->PlayerIndex, 0, static_cast<int32_t>(Input::MaxPlayers) - 1)));
	}
}
