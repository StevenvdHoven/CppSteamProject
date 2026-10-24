#pragma once
#include "Core/Core.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Flex
{
	class World;

	enum class SystemPhase : uint8_t
	{
		Start,       // once, when the game starts (after the scene is loaded)
		FixedUpdate, // every fixed step (PhysicsSettings::FixedTimeStep), before physics
		Update       // every frame, after the fixed steps
	};

	// A system is a plain function over the world. deltaTime is the fixed step in FixedUpdate,
	// the frame time in Update and 0 in Start. Structural changes inside World::Each go through
	// world.GetCommands(); the game applies them after every system.
	using SystemFunction = void (*)(World& world, float deltaTime);

	struct SystemInfo
	{
		std::string Name;
		SystemPhase Phase = SystemPhase::Update;
		SystemFunction Function = nullptr;
		int Order = 0; // lower runs first; systems with the same order run by name
	};

	// Process-wide list of game systems, filled by FLEX_REGISTER_SYSTEM when a module loads.
	class FLEX_API SystemRegistery
	{
	public:
		static void Register(SystemPhase phase, const char* name, SystemFunction function, int order = 0);

		// The systems of one phase, sorted by order, then by name (a stable order whatever the load order).
		static std::vector<SystemInfo> GetSystems(SystemPhase phase);

		// Removes every system matching the predicate (before its module is unloaded).
		static size_t Remove(const std::function<bool(const SystemInfo&)>& predicate);
	};

	// Registers a system function when its module loads (put it next to the function):
	//
	//     void MoveBullets(World& world, float deltaTime) { ... }
	//     FLEX_REGISTER_SYSTEM(Update, MoveBullets)
	#define FLEX_REGISTER_SYSTEM(Phase, Function) \
		static const bool FlexSystemRegistered_##Function = (::Flex::SystemRegistery::Register(::Flex::SystemPhase::Phase, #Function, &Function), true);

	// Same, with an explicit order within the phase (default 0; lower runs first). For systems that
	// must run before or after the game's own systems, e.g. a network pump with a negative order:
	//
	//     FLEX_REGISTER_SYSTEM_ORDER(Update, PumpNetwork, -1000)
	#define FLEX_REGISTER_SYSTEM_ORDER(Phase, Function, Order) \
		static const bool FlexSystemRegistered_##Function = (::Flex::SystemRegistery::Register(::Flex::SystemPhase::Phase, #Function, &Function, Order), true);
}
