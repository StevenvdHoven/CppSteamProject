#pragma once
#include "Plugins/Steam/Runtime/SteamTypes.h"
#include "Core/Math/Color.h"
#include "Core/Math/MathUtils.h"
#include "Core/Math/Vector2.h"
#include "Core/Uid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace Flex::Steam
{
	class PacketWriter;
	class PacketReader;

	// A struct with a template Serialize(Stream&) member: packets, and structs used inside packets.
	// One function writes and reads, so both sides always agree:
	//
	//     template<typename Stream> void Serialize(Stream& stream) { stream.Value(Text); stream.Value(Position); }
	//
	// Branch on Stream::bIsReading when the two directions must differ.
	template<typename T>
	concept HasPacketSerialize = requires(T& value, PacketWriter& writer, PacketReader& reader) {
		value.Serialize(writer);
		value.Serialize(reader);
	};

	// Numbers and enums are copied as they are.
	template<typename T>
	concept IsRawPacketValue = (std::is_arithmetic_v<T> || std::is_enum_v<T>) && !std::is_same_v<T, bool>;

	// Appends a packet's values to a byte buffer: little-endian, byte-aligned, no names or types, so both
	// sides must run the same Serialize (builds with different packets are refused by the protocol hash).
	// Header-only so Serialize inlines into one tight function per packet type.
	class PacketWriter
	{
	public:
		static constexpr bool bIsWriting = true;
		static constexpr bool bIsReading = false;

		explicit PacketWriter(std::vector<std::byte>& buffer) : Buffer{ buffer } {}

		void Bytes(const void* data, size_t size)
		{
			if (size == 0) {
				return;
			}
			const size_t offset = Buffer.size();
			Buffer.resize(offset + size);
			std::memcpy(Buffer.data() + offset, data, size);
		}

		template<typename T>
		void Value(const T& value)
		{
			if constexpr (std::is_same_v<T, bool>) {
				const uint8_t byte = value ? 1 : 0;
				Bytes(&byte, 1);
			} else if constexpr (IsRawPacketValue<T>) {
				Bytes(&value, sizeof(T));
			} else if constexpr (HasPacketSerialize<T>) {
				const_cast<T&>(value).Serialize(*this); // Serialize only reads the value while writing
			} else {
				static_assert(sizeof(T) == 0, "This type can't go into a packet: give it a template Serialize(Stream&) member.");
			}
		}

		// Length-prefixed (VarUInt) UTF-8.
		void Value(const std::string& value)
		{
			VarUInt(static_cast<uint64_t>(value.size()));
			Bytes(value.data(), value.size());
		}

		// Count-prefixed (VarUInt); numbers are copied as one block.
		template<typename T>
		void Value(const std::vector<T>& values)
		{
			VarUInt(static_cast<uint64_t>(values.size()));
			if constexpr (IsRawPacketValue<T>) {
				Bytes(values.data(), values.size() * sizeof(T));
			} else {
				for (const T& value : values) {
					Value(value);
				}
			}
		}

		template<typename T, size_t Count>
		void Value(const std::array<T, Count>& values)
		{
			if constexpr (IsRawPacketValue<T>) {
				Bytes(values.data(), Count * sizeof(T));
			} else {
				for (const T& value : values) {
					Value(value);
				}
			}
		}

		void Value(const Vector2& value) { Value(value.x); Value(value.y); }
		void Value(const Vector2Int& value) { Value(value.x); Value(value.y); }
		void Value(const Color& value) { Value(value.r); Value(value.g); Value(value.b); Value(value.a); }
		void Value(const Uid& value) { Value(value.GetValue()); }
		void Value(const SteamId& value) { Value(value.Value); }

		// 1 byte for 0-127, 2 for up to 16383, ... (counts, ids, small numbers).
		template<std::unsigned_integral T>
		void VarUInt(const T& value)
		{
			uint64_t remaining = value;
			uint8_t bytes[10];
			size_t count = 0;
			do {
				uint8_t byte = static_cast<uint8_t>(remaining & 0x7F);
				remaining >>= 7;
				if (remaining != 0) {
					byte |= 0x80;
				}
				bytes[count++] = byte;
			} while (remaining != 0);
			Bytes(bytes, count);
		}

		// Like VarUInt, small negative numbers stay small too (zigzag).
		template<std::signed_integral T>
		void VarInt(const T& value)
		{
			const int64_t wide = value;
			VarUInt((static_cast<uint64_t>(wide) << 1) ^ static_cast<uint64_t>(wide >> 63));
		}

		// A float in [min, max] stored as a Quantized integer (uint8_t, uint16_t or uint32_t): values outside
		// are clamped, the step is (max - min) / Quantized's max.
		template<std::unsigned_integral Quantized>
		void QuantizedFloat(const float& value, float min, float max)
		{
			const float normalized = std::clamp((value - min) / (max - min), 0.0f, 1.0f);
			const Quantized quantized = static_cast<Quantized>(std::lround(normalized * static_cast<float>(std::numeric_limits<Quantized>::max())));
			Value(quantized);
		}

		// An angle in radians, wrapped to (-Pi, Pi] and quantized (uint16_t: steps of ~0.0055 degrees).
		template<std::unsigned_integral Quantized>
		void Angle(const float& radians)
		{
			QuantizedFloat<Quantized>(Math::WrapAngle(radians), -Math::Pi, Math::Pi);
		}

		size_t GetSize() const { return Buffer.size(); }

	private:
		std::vector<std::byte>& Buffer;
	};

	// Reads what PacketWriter wrote. Never trusts the data: reading past the end, a length longer than
	// the rest or a value that doesn't fit marks the reader failed, and from then on every read leaves
	// its value unchanged. The caller drops a packet whose reader failed.
	class PacketReader
	{
	public:
		static constexpr bool bIsWriting = false;
		static constexpr bool bIsReading = true;

		explicit PacketReader(std::span<const std::byte> data) : Data{ data } {}

		bool Bytes(void* outData, size_t size)
		{
			if (bFailed || size > GetRemaining()) {
				bFailed = true;
				return false;
			}
			if (size != 0) {
				std::memcpy(outData, Data.data() + Offset, size);
				Offset += size;
			}
			return true;
		}

		template<typename T>
		void Value(T& value)
		{
			if constexpr (std::is_same_v<T, bool>) {
				uint8_t byte = 0;
				if (Bytes(&byte, 1)) {
					value = byte != 0;
				}
			} else if constexpr (IsRawPacketValue<T>) {
				T read;
				if (Bytes(&read, sizeof(T))) {
					value = read;
				}
			} else if constexpr (HasPacketSerialize<T>) {
				value.Serialize(*this);
			} else {
				static_assert(sizeof(T) == 0, "This type can't go into a packet: give it a template Serialize(Stream&) member.");
			}
		}

		void Value(std::string& value)
		{
			uint64_t length = 0;
			VarUInt(length);
			if (bFailed || length > GetRemaining()) {
				bFailed = true;
				return;
			}
			value.assign(reinterpret_cast<const char*>(Data.data() + Offset), static_cast<size_t>(length));
			Offset += static_cast<size_t>(length);
		}

		// Every element takes at least one byte, so a count larger than the rest fails at once
		// (a corrupt count can't make it allocate gigabytes).
		template<typename T>
		void Value(std::vector<T>& values)
		{
			uint64_t count = 0;
			VarUInt(count);
			const uint64_t elementSize = IsRawPacketValue<T> ? sizeof(T) : 1;
			if (bFailed || count > GetRemaining() / elementSize) {
				bFailed = true;
				return;
			}
			values.resize(static_cast<size_t>(count));
			if constexpr (IsRawPacketValue<T>) {
				Bytes(values.data(), values.size() * sizeof(T));
			} else {
				for (T& value : values) {
					Value(value);
					if (bFailed) {
						return;
					}
				}
			}
		}

		template<typename T, size_t Count>
		void Value(std::array<T, Count>& values)
		{
			if constexpr (IsRawPacketValue<T>) {
				Bytes(values.data(), Count * sizeof(T));
			} else {
				for (T& value : values) {
					Value(value);
				}
			}
		}

		void Value(Vector2& value) { Value(value.x); Value(value.y); }
		void Value(Vector2Int& value) { Value(value.x); Value(value.y); }
		void Value(Color& value) { Value(value.r); Value(value.g); Value(value.b); Value(value.a); }

		void Value(Uid& value)
		{
			uint64_t raw = value.GetValue();
			Value(raw);
			value = Uid{ raw };
		}

		void Value(SteamId& value) { Value(value.Value); }

		template<std::unsigned_integral T>
		void VarUInt(T& value)
		{
			uint64_t result = 0;
			for (uint32_t shift = 0; shift < 64; shift += 7) {
				uint8_t byte = 0;
				if (!Bytes(&byte, 1)) {
					return;
				}
				result |= static_cast<uint64_t>(byte & 0x7F) << shift;
				if ((byte & 0x80) == 0) {
					if (result > std::numeric_limits<T>::max()) {
						bFailed = true;
						return;
					}
					value = static_cast<T>(result);
					return;
				}
			}
			bFailed = true; // more than 10 bytes: not a VarUInt
		}

		template<std::signed_integral T>
		void VarInt(T& value)
		{
			uint64_t zigzag = 0;
			VarUInt(zigzag);
			if (bFailed) {
				return;
			}
			const int64_t wide = static_cast<int64_t>(zigzag >> 1) ^ -static_cast<int64_t>(zigzag & 1);
			if (wide < std::numeric_limits<T>::min() || wide > std::numeric_limits<T>::max()) {
				bFailed = true;
				return;
			}
			value = static_cast<T>(wide);
		}

		template<std::unsigned_integral Quantized>
		void QuantizedFloat(float& value, float min, float max)
		{
			Quantized quantized = 0;
			if (Bytes(&quantized, sizeof(Quantized))) {
				value = min + (max - min) * (static_cast<float>(quantized) / static_cast<float>(std::numeric_limits<Quantized>::max()));
			}
		}

		template<std::unsigned_integral Quantized>
		void Angle(float& radians)
		{
			QuantizedFloat<Quantized>(radians, -Math::Pi, Math::Pi);
		}

		bool IsFailed() const { return bFailed; }
		size_t GetRemaining() const { return Data.size() - Offset; }
		bool IsAtEnd() const { return Offset == Data.size(); }

		// For a Serialize that finds a value it doesn't accept (e.g. an enum out of range).
		void Fail() { bFailed = true; }

	private:
		std::span<const std::byte> Data;
		size_t Offset = 0;
		bool bFailed = false;
	};
}
