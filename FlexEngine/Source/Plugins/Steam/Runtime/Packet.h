#pragma once
#include "Plugins/Steam/Runtime/PacketRegistery.h"
#include "Plugins/Steam/Runtime/PacketStream.h"
#include "Plugins/Steam/Runtime/SteamApi.h"
#include "Plugins/Steam/Runtime/SteamTypes.h"

#include <concepts>
#include <cstddef>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Flex::Steam
{
	// A packet is a plain struct marked with FLEX_STEAM_PACKET(Name) (no base class, no vtable):
	//
	//     struct ChatMessage
	//     {
	//         FLEX_STEAM_PACKET(ChatMessage)
	//         std::string Text;
	//
	//         template<typename Stream> void Serialize(Stream& stream) { stream.Value(Text); }
	//
	//         // Optional, all with defaults:
	//         static constexpr Steam::Delivery PacketDelivery = Steam::Delivery::Reliable;
	//         static constexpr Steam::Authority PacketAuthority = Steam::Authority::Anyone;
	//         static constexpr uint32_t PacketVersion = 1; // bump when the fields change
	//
	//         // Network thread (must not touch the World). Without it: ToMainThread when OnReceiveMain exists.
	//         Steam::HandleResult OnReceive(const Steam::PacketContext& context);
	//         // Main thread, when OnReceive returned ToMainThread.
	//         void OnReceiveMain(World& world, const Steam::PacketContext& context);
	//     };
	//
	// and registered in one .cpp with FLEX_REGISTER_STEAM_PACKET(ChatMessage), so it can be received before
	// it was ever sent. The name identifies the type on the wire, so it must be unique in the game.
	// A trivially copyable packet may leave out Serialize: it's then copied as one block.
	#define FLEX_STEAM_PACKET(TypeName) static constexpr const char* PacketName = #TypeName;

	template<typename T>
	concept IsPacket = requires { { T::PacketName } -> std::convertible_to<const char*>; }
		&& !std::is_const_v<T> && !std::is_reference_v<T>
		&& std::is_default_constructible_v<T> && std::is_copy_constructible_v<T> && std::is_destructible_v<T>;

	namespace Detail
	{
		template<typename T>
		concept HasOnReceive = requires(T& packet, const PacketContext& context) {
			{ packet.OnReceive(context) } -> std::same_as<HandleResult>;
		};

		template<typename T>
		concept HasOnReceiveMain = requires(T& packet, World& world, const PacketContext& context) {
			packet.OnReceiveMain(world, context);
		};

		template<typename T>
		constexpr Delivery GetDelivery()
		{
			if constexpr (requires { T::PacketDelivery; }) {
				return T::PacketDelivery;
			} else {
				return Delivery::Reliable;
			}
		}

		template<typename T>
		constexpr Authority GetAuthority()
		{
			if constexpr (requires { T::PacketAuthority; }) {
				return T::PacketAuthority;
			} else {
				return Authority::Anyone;
			}
		}

		template<typename T>
		constexpr uint32_t GetVersion()
		{
			if constexpr (requires { T::PacketVersion; }) {
				return T::PacketVersion;
			} else {
				return 1;
			}
		}

		template<IsPacket T>
		PacketTypeInfo MakePacketTypeInfo()
		{
			// A handler with the wrong signature would silently never run: catch it here.
			if constexpr (requires { &T::OnReceive; }) {
				static_assert(HasOnReceive<T>, "OnReceive must be: Steam::HandleResult OnReceive(const Steam::PacketContext& context)");
			}
			if constexpr (requires { &T::OnReceiveMain; }) {
				static_assert(HasOnReceiveMain<T>, "OnReceiveMain must be: void OnReceiveMain(World& world, const Steam::PacketContext& context)");
			}
			if constexpr (!HasPacketSerialize<T>) {
				static_assert(std::is_trivially_copyable_v<T>, "This packet needs a template Serialize(Stream&) member (only trivially copyable packets may leave it out).");
			}

			PacketTypeInfo info;
			info.Name = T::PacketName;
			info.Id = HashPacketName(T::PacketName);
			info.Version = GetVersion<T>();
			info.Size = static_cast<uint32_t>(sizeof(T));
			info.Alignment = static_cast<uint32_t>(alignof(T));
			info.DefaultDelivery = GetDelivery<T>();
			info.SenderAuthority = GetAuthority<T>();

			info.Create = []() -> void* { return new T(); };
			info.Clone = [](const void* packet) -> void* { return new T(*static_cast<const T*>(packet)); };
			info.Destroy = [](void* packet) { delete static_cast<T*>(packet); };
			info.Write = [](const void* packet, PacketWriter& writer) {
				if constexpr (HasPacketSerialize<T>) {
					writer.Value(*static_cast<const T*>(packet));
				} else {
					writer.Bytes(packet, sizeof(T));
				}
			};
			info.Read = [](void* packet, PacketReader& reader) {
				if constexpr (HasPacketSerialize<T>) {
					reader.Value(*static_cast<T*>(packet));
				} else {
					reader.Bytes(packet, sizeof(T));
				}
			};
			info.Receive = [](void* packet, const PacketContext& context) -> HandleResult {
				if constexpr (HasOnReceive<T>) {
					return static_cast<T*>(packet)->OnReceive(context);
				} else {
					(void)packet;
					(void)context;
					return HasOnReceiveMain<T> ? HandleResult::ToMainThread : HandleResult::Done;
				}
			};
			if constexpr (HasOnReceiveMain<T>) {
				info.ReceiveMain = [](void* packet, World& world, const PacketContext& context) {
					static_cast<T*>(packet)->OnReceiveMain(world, context);
				};
			}
			return info;
		}
	}

	// The registered description of a packet type (registers it on first use).
	template<IsPacket T>
	const PacketTypeInfo& GetPacketType()
	{
		static const PacketTypeInfo* const type = PacketRegistery::Register(Detail::MakePacketTypeInfo<T>());
		return *type;
	}

	// Registers a packet type when its module loads (put it in one .cpp file, next to the packet).
	#define FLEX_REGISTER_STEAM_PACKET(Type) \
		static const bool FlexSteamPacketRegistered_##Type = (::Flex::Steam::GetPacketType<Type>(), true);

	// Owns one packet of any type (the queues between the main and the network thread hold these).
	// A handle must be gone before the module that defines its type unloads.
	class PacketHandle
	{
	public:
		PacketHandle() = default;
		PacketHandle(const PacketTypeInfo* type, void* packet) : Type{ type }, Packet{ packet } {}

		PacketHandle(PacketHandle&& other) noexcept
			: Type{ std::exchange(other.Type, nullptr) }, Packet{ std::exchange(other.Packet, nullptr) }
		{
		}

		PacketHandle& operator=(PacketHandle&& other) noexcept
		{
			if (this != &other) {
				Reset();
				Type = std::exchange(other.Type, nullptr);
				Packet = std::exchange(other.Packet, nullptr);
			}
			return *this;
		}

		PacketHandle(const PacketHandle&) = delete;
		PacketHandle& operator=(const PacketHandle&) = delete;

		~PacketHandle() { Reset(); }

		// Takes the packet by value: pass std::move(packet) to avoid the copy.
		template<IsPacket T>
		static PacketHandle Make(T packet)
		{
			return PacketHandle{ &GetPacketType<T>(), new T(std::move(packet)) };
		}

		void Reset()
		{
			if (Packet) {
				Type->Destroy(Packet);
			}
			Type = nullptr;
			Packet = nullptr;
		}

		const PacketTypeInfo* GetType() const { return Type; }
		void* Get() const { return Packet; }

		// The packet when it is a T, else nullptr.
		template<IsPacket T>
		T* As() const
		{
			return Packet && Type->Id == HashPacketName(T::PacketName) ? static_cast<T*>(Packet) : nullptr;
		}

		explicit operator bool() const { return Packet != nullptr; }

	private:
		const PacketTypeInfo* Type = nullptr;
		void* Packet = nullptr;
	};

	// Wire format of one packet: [PacketTypeId, 4 bytes][the packet's Serialize]. Appended to outBytes.
	FLEX_STEAM_API void EncodePacket(const PacketTypeInfo& type, const void* packet, std::vector<std::byte>& outBytes);

	template<IsPacket T>
	void EncodePacket(const T& packet, std::vector<std::byte>& outBytes)
	{
		EncodePacket(GetPacketType<T>(), &packet, outBytes);
	}

	// The packet in bytes, or an empty handle (and why in outError) when its type isn't registered or the
	// bytes don't match its Serialize (cut short, longer than it reads, corrupt). Never trusts the data.
	FLEX_STEAM_API PacketHandle DecodePacket(std::span<const std::byte> bytes, std::string* outError = nullptr);
}
