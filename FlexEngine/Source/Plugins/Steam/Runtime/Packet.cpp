#include "Plugins/Steam/Runtime/Packet.h"
#include <cstdio>

namespace Flex::Steam
{
	void EncodePacket(const PacketTypeInfo& type, const void* packet, std::vector<std::byte>& outBytes)
	{
		PacketWriter writer{ outBytes };
		writer.Value(type.Id);
		type.Write(packet, writer);
	}

	PacketHandle DecodePacket(std::span<const std::byte> bytes, std::string* outError)
	{
		const auto fail = [outError](std::string message) {
			if (outError) {
				*outError = std::move(message);
			}
			return PacketHandle{};
		};

		PacketReader reader{ bytes };
		PacketTypeId id = 0;
		reader.Value(id);
		if (reader.IsFailed()) {
			return fail("The packet is shorter than its header.");
		}
		const PacketTypeInfo* type = PacketRegistery::Find(id);
		if (!type) {
			char text[64];
			std::snprintf(text, sizeof(text), "Unknown packet type %08X.", id);
			return fail(text);
		}

		PacketHandle packet{ type, type->Create() };
		type->Read(packet.Get(), reader);
		if (reader.IsFailed()) {
			return fail(type->Name + ": the data is cut short or corrupt.");
		}
		if (!reader.IsAtEnd()) {
			return fail(type->Name + ": " + std::to_string(reader.GetRemaining()) + " byte(s) left over (do both sides run the same build?).");
		}
		return packet;
	}
}
