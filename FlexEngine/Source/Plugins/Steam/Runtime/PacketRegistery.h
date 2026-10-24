#pragma once
#include "Plugins/Steam/Runtime/SteamApi.h"
#include "Plugins/Steam/Runtime/SteamTypes.h"
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Flex
{
	class World;
}

namespace Flex::Steam
{
	class PacketReader;
	class PacketWriter;

	// Sent in front of every packet. The hash of the packet's name, so it's the same in every build
	// and on every peer without any id table (Register asserts on the rare collision: rename one).
	using PacketTypeId = uint32_t;

	// FNV-1a, 32 bits.
	constexpr PacketTypeId HashPacketName(std::string_view name)
	{
		uint32_t hash = 2166136261u;
		for (const char character : name) {
			hash = (hash ^ static_cast<uint8_t>(character)) * 16777619u;
		}
		return hash;
	}

	// Type-erased description of a packet type, made by GetPacketType<T>() (Packet.h). The functions live
	// in the module that defines the packet; the description itself lives in FlexSteam for the whole run.
	struct PacketTypeInfo
	{
		std::string Name;
		PacketTypeId Id = 0;
		uint32_t Version = 1; // PacketVersion: bump when the fields change, so old builds are refused
		uint32_t Size = 0;
		uint32_t Alignment = 0;
		Delivery DefaultDelivery = Delivery::Reliable;     // PacketDelivery
		Authority SenderAuthority = Authority::Anyone;     // PacketAuthority

		void* (*Create)() = nullptr;                       // a default-constructed packet (new)
		void* (*Clone)(const void* packet) = nullptr;      // a copy (new)
		void (*Destroy)(void* packet) = nullptr;           // delete
		void (*Write)(const void* packet, PacketWriter& writer) = nullptr;
		void (*Read)(void* packet, PacketReader& reader) = nullptr;
		// Network thread: the packet's OnReceive, or the default (ToMainThread when it has OnReceiveMain).
		HandleResult (*Receive)(void* packet, const PacketContext& context) = nullptr;
		// Main thread: the packet's OnReceiveMain; nullptr when it has none.
		void (*ReceiveMain)(void* packet, World& world, const PacketContext& context) = nullptr;

		bool bIsStale = false; // the module that defined it was unloaded (hot reload)
	};

	// Every packet type of the process, registered by FLEX_REGISTER_STEAM_PACKET when a module loads.
	// Thread-safe: the network thread looks types up while the main thread registers.
	//
	// Hot reload: before the game module unloads, its packet types are marked stale (they stay allocated,
	// but aren't found any more); the rebuilt module registers them again.
	class FLEX_STEAM_API PacketRegistery
	{
	public:
		// Adds the type, or returns the live type with the same name (asserts when its size differs).
		// Asserts when another live type has the same id (hash collision).
		static const PacketTypeInfo* Register(const PacketTypeInfo& info);

		// nullptr when unknown or stale.
		static const PacketTypeInfo* Find(PacketTypeId id);
		static const PacketTypeInfo* FindByName(std::string_view name);

		// The live types, sorted by name.
		static std::vector<const PacketTypeInfo*> GetTypes();

		// Hash of every live type's name and version. Two builds can talk when theirs are equal.
		static uint64_t GetProtocolHash();

		// Marks every live type matching the predicate stale. Returns how many.
		static size_t MarkStale(const std::function<bool(const PacketTypeInfo&)>& predicate);
	};
}
