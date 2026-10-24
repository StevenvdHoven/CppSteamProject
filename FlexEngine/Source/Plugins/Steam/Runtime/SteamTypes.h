#pragma once
#include "Plugins/Steam/Runtime/SteamApi.h"
#include <cstdint>
#include <functional>

namespace Flex::Steam
{
	// A Steam user (Steam's CSteamID as a 64-bit number), so game code never needs the Steamworks headers.
	struct SteamId
	{
		uint64_t Value = 0;

		constexpr SteamId() = default;
		constexpr explicit SteamId(uint64_t value) : Value{ value } {}

		constexpr bool IsValid() const { return Value != 0; }
		constexpr bool operator==(const SteamId&) const = default;
	};

	// How a packet travels.
	enum class Delivery : uint8_t
	{
		Reliable,         // arrives exactly once, in order (chat, spawns, kicks). The default.
		Unreliable,       // may be lost or arrive out of order; cheapest (state sent often, like transforms)
		UnreliableNoDelay // like Unreliable, but dropped instead of queued when it can't go out at once
	};

	// Who may send a packet. Checked on the network thread before any of the packet's code runs.
	enum class Authority : uint8_t
	{
		Anyone,  // the host and every client. The default.
		HostOnly // only accepted from the host (game state, kicks)
	};

	// What happens after a packet's OnReceive (network thread).
	enum class HandleResult : uint8_t
	{
		Done,        // handled; the packet is freed
		ToMainThread // also run OnReceiveMain on the main thread (needed to touch the World)
	};

	// Passed to a packet's handlers.
	struct PacketContext
	{
		SteamId Sender;           // who sent it (the local user for loopback)
		bool bFromHost = false;
		bool bIsLoopback = false; // sent by this process to itself (the host is also a player)
	};
}

template<>
struct std::hash<Flex::Steam::SteamId>
{
	size_t operator()(const Flex::Steam::SteamId& id) const noexcept
	{
		return std::hash<uint64_t>{}(id.Value);
	}
};
