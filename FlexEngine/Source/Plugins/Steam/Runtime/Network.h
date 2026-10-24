#pragma once
#include "Plugins/Steam/Runtime/Packet.h"
#include "Plugins/Steam/Runtime/SteamApi.h"
#include "Plugins/Steam/Runtime/SteamTypes.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Flex
{
	class World;
}

namespace Flex::Steam
{
	// Who a packet goes to. Clients only have a connection to the host: what a client sends to others
	// travels through the host, which checks it and passes it on (the receivers see the client as Sender).
	struct Target
	{
		enum class Kind : uint8_t
		{
			Host,     // the host (from the host itself: handled locally)
			Everyone, // every member, yourself included (locally)
			Others,   // every member except yourself
			User      // one member (yourself: locally)
		};

		Kind Type = Kind::Host;
		SteamId User;

		static Target ToHost() { return Target{ Kind::Host, {} }; }
		static Target ToEveryone() { return Target{ Kind::Everyone, {} }; }
		static Target ToOthers() { return Target{ Kind::Others, {} }; }
		static Target ToUser(SteamId user) { return Target{ Kind::User, user }; }
	};

	struct PacketTypeStats
	{
		std::string Name;
		uint64_t Sent = 0;
		uint64_t Received = 0;
		uint64_t BytesSent = 0;
		uint64_t BytesReceived = 0;
	};

	struct NetworkStats
	{
		uint64_t PacketsSent = 0;     // per connection a packet went out on (loopback not counted)
		uint64_t PacketsReceived = 0; // from connections
		uint64_t BytesSent = 0;
		uint64_t BytesReceived = 0;
		uint64_t PacketsRelayed = 0;  // host: passed on from one client to others
		uint64_t PacketsHandledLocally = 0; // loopback
		uint64_t PacketsDropped = 0;  // malformed, unknown type, not allowed from that sender, ...
		std::vector<PacketTypeStats> Types; // by name
	};

	struct ConnectionInfo
	{
		SteamId User;
		bool bIsConnected = false; // false while connecting
		int PingMilliseconds = -1;
		float Quality = 0.0f;      // 0-1, fraction of packets delivered
		uint32_t PendingReliableBytes = 0;
		uint32_t PendingUnreliableBytes = 0;
	};

	// Packets between the members of the lobby, over Steam's P2P sockets (Steam Datagram Relay: no
	// open ports, no IP addresses shared). The host is the hub: clients connect to the host only.
	//
	// A network thread does the work: it turns queued packets into bytes and sends them, receives and
	// decodes incoming packets, checks who may send them (PacketAuthority), and runs their OnReceive.
	// Packets that want the main thread (OnReceiveMain) are queued and run by DispatchMainThread, which
	// the SteamUpdate system calls first thing every frame.
	//
	// The network starts when you enter a lobby and stops when you leave. The host only accepts
	// connections from lobby members who aren't banned; a kick also closes the connection.
	class FLEX_STEAM_API Network
	{
	public:
		// Main thread. The packet is moved into the queue (pass std::move(packet) to avoid a copy) and
		// turned into bytes on the network thread. False when not in a lobby, when a client sends a
		// HostOnly packet, or when the target user isn't a member.
		template<IsPacket T>
		static bool Send(T packet, Target target)
		{
			const Delivery delivery = GetPacketType<T>().DefaultDelivery;
			return SendHandle(PacketHandle::Make(std::move(packet)), target, delivery);
		}

		template<IsPacket T>
		static bool Send(T packet, Target target, Delivery delivery)
		{
			return SendHandle(PacketHandle::Make(std::move(packet)), target, delivery);
		}

		static bool SendHandle(PacketHandle packet, Target target, Delivery delivery);

		// Main thread: runs OnReceiveMain of every packet queued for it. Without a world (the editor
		// while not playing) the packets are dropped.
		static void DispatchMainThread(World* world);

		static bool IsActive();    // in a lobby, network running
		static bool IsConnected(); // host: always while active; client: connected to the host

		static NetworkStats GetStats();
		static std::vector<ConnectionInfo> GetConnections(); // host: every client; client: the host
	};

#if FLEX_DEBUG_TOOLS
	namespace Detail
	{
		// Tests only (host): treats an existing connection (e.g. from CreateSocketPair) as an accepted client.
		FLEX_STEAM_API void NetworkTestAddClient(SteamId user, uint32_t connection);
	}
#endif
}
