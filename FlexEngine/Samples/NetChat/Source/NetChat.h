#pragma once
#include "Plugins/Steam/Runtime/SteamNetwork.h"
#include "Core/Math/Vector2.h"
#include "Core/Serialization/Archive.h"
#include "Core/Uid.h"
#include "Core/World/ComponentRegistery.h"

#include <cstdint>
#include <string>

// The NetChat sample: the Steam plugin used the way a game uses it.
//
//   F1 create a lobby (you're the host)   F2 invite friends (Steam overlay)   F3 leave
//   1 / 2 / 3 send a chat line             arrow keys, left stick or D-pad steer the ship
//
// The host is the source of truth: everyone sends their steering to the host (SteerInput), only the
// host moves the ship, and the host sends its position to everyone else (ShipState, HostOnly).
// Chat lines are handled on the network thread (they don't touch the world).
namespace NetChat
{
	// A ship everyone in the lobby steers together.
	struct ShipControl
	{
		FLEX_COMPONENT(ShipControl)
		static constexpr uint32_t Version = 1;

		float Speed = 220.0f; // units per second

		// Runtime state (not saved): the steering received this fixed step, from every member.
		Flex::Vector2 Input;

		void Serialize(Flex::Archive& archive)
		{
			archive.SerializeVersion(Version);
			archive.Field("Speed", Speed);
		}
	};

	// A chat line. Reliable, anyone may send it, handled on the network thread.
	struct ChatMessage
	{
		FLEX_STEAM_PACKET(ChatMessage)
		std::string Text;

		template<typename Stream>
		void Serialize(Stream& stream)
		{
			stream.Value(Text);
		}

		Flex::Steam::HandleResult OnReceive(const Flex::Steam::PacketContext& context);
	};

	// One member's steering for one fixed step, sent to the host. Lost ones don't matter.
	struct SteerInput
	{
		FLEX_STEAM_PACKET(SteerInput)
		static constexpr Flex::Steam::Delivery PacketDelivery = Flex::Steam::Delivery::UnreliableNoDelay;

		int8_t X = 0; // -1, 0 or 1
		int8_t Y = 0;

		void OnReceiveMain(Flex::World& world, const Flex::Steam::PacketContext& context);
	};

	// Where the host put a ship. Only the host may send it; clients apply it on the main thread.
	struct ShipState
	{
		FLEX_STEAM_PACKET(ShipState)
		static constexpr Flex::Steam::Delivery PacketDelivery = Flex::Steam::Delivery::Unreliable;
		static constexpr Flex::Steam::Authority PacketAuthority = Flex::Steam::Authority::HostOnly;

		Flex::Uid Ship; // the ship's GameObject id: the same on every member (same scene file)
		Flex::Vector2 Position;
		float Rotation = 0.0f;

		template<typename Stream>
		void Serialize(Stream& stream)
		{
			stream.Value(Ship);
			stream.Value(Position);
			stream.template Angle<uint16_t>(Rotation);
		}

		void OnReceiveMain(Flex::World& world, const Flex::Steam::PacketContext& context);
	};
}
