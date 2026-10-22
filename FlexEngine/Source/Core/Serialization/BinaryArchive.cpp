#include "Core/Serialization/BinaryArchive.h"
#include <bit>
#include <cstring>

namespace Flex
{
	// Values are copied as-is, so the format is little-endian (every platform we target).
	static_assert(std::endian::native == std::endian::little, "BinaryArchive assumes a little-endian platform.");

	// -----------------------
	// BinaryWriter
	// -----------------------

	BinaryWriter::BinaryWriter(std::vector<std::byte>& buffer) :
		Archive(ArchiveMode::Save),
		Buffer{ buffer }
	{
	}

	void BinaryWriter::WriteBytes(const void* data, size_t size)
	{
		const std::byte* bytes = static_cast<const std::byte*>(data);
		Buffer.insert(Buffer.end(), bytes, bytes + size);
	}

	uint32_t BinaryWriter::SerializeVersion(uint32_t currentVersion)
	{
		WriteBytes(&currentVersion, sizeof(currentVersion));
		return currentVersion;
	}

	void BinaryWriter::Field(const char*, bool& value)
	{
		const uint8_t byte = value ? 1 : 0;
		WriteBytes(&byte, sizeof(byte));
	}

	void BinaryWriter::Field(const char*, int32_t& value) { WriteBytes(&value, sizeof(value)); }
	void BinaryWriter::Field(const char*, uint32_t& value) { WriteBytes(&value, sizeof(value)); }
	void BinaryWriter::Field(const char*, int64_t& value) { WriteBytes(&value, sizeof(value)); }
	void BinaryWriter::Field(const char*, float& value) { WriteBytes(&value, sizeof(value)); }
	void BinaryWriter::Field(const char*, double& value) { WriteBytes(&value, sizeof(value)); }

	void BinaryWriter::Field(const char*, std::string& value)
	{
		const uint32_t size = static_cast<uint32_t>(value.size());
		WriteBytes(&size, sizeof(size));
		WriteBytes(value.data(), value.size());
	}

	void BinaryWriter::Field(const char*, Uid& value)
	{
		const uint64_t raw = value.GetValue();
		WriteBytes(&raw, sizeof(raw));
	}

	// -----------------------
	// BinaryReader
	// -----------------------

	BinaryReader::BinaryReader(std::span<const std::byte> data) :
		Archive(ArchiveMode::Load),
		Data{ data }
	{
	}

	bool BinaryReader::ReadBytes(void* outData, size_t size, const char* name)
	{
		if (bFailed) {
			return false;
		}
		if (Offset + size > Data.size()) {
			bFailed = true;
			AddWarning(std::string("Binary data ended while reading '") + name + "'.");
			return false;
		}
		std::memcpy(outData, Data.data() + Offset, size);
		Offset += size;
		return true;
	}

	uint32_t BinaryReader::SerializeVersion(uint32_t)
	{
		uint32_t version = 0;
		ReadBytes(&version, sizeof(version), "version");
		return version;
	}

	void BinaryReader::Field(const char* name, bool& value)
	{
		uint8_t byte = 0;
		if (ReadBytes(&byte, sizeof(byte), name)) {
			value = byte != 0;
		}
	}

	void BinaryReader::Field(const char* name, int32_t& value) { ReadBytes(&value, sizeof(value), name); }
	void BinaryReader::Field(const char* name, uint32_t& value) { ReadBytes(&value, sizeof(value), name); }
	void BinaryReader::Field(const char* name, int64_t& value) { ReadBytes(&value, sizeof(value), name); }
	void BinaryReader::Field(const char* name, float& value) { ReadBytes(&value, sizeof(value), name); }
	void BinaryReader::Field(const char* name, double& value) { ReadBytes(&value, sizeof(value), name); }

	void BinaryReader::Field(const char* name, std::string& value)
	{
		uint32_t size = 0;
		if (!ReadBytes(&size, sizeof(size), name)) {
			return;
		}

		// Check before allocating, so corrupt data can't request gigabytes.
		if (size > Data.size() - Offset) {
			bFailed = true;
			AddWarning(std::string("Binary data ended while reading '") + name + "'.");
			return;
		}

		std::string result(size, '\0');
		if (ReadBytes(result.data(), size, name)) {
			value = std::move(result);
		}
	}

	void BinaryReader::Field(const char* name, Uid& value)
	{
		uint64_t raw = 0;
		if (ReadBytes(&raw, sizeof(raw), name)) {
			value = Uid(raw);
		}
	}
}
