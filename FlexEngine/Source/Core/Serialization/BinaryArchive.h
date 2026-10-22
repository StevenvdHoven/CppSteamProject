#pragma once
#include "Core/Serialization/Archive.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Flex
{
	// Compact binary archive for cooked data in exported games.
	// Field names are NOT stored: fields must be read in exactly the order they were written.
	// That's fine for cooked data, because it's rebuilt by the same engine version on every export.
	// Never use it for files in a project (use TextArchive there).
	class FLEX_API BinaryWriter final : public Archive
	{
	public:
		explicit BinaryWriter(std::vector<std::byte>& buffer);

		using Archive::Field; // keep the math overloads visible next to the overrides

		uint32_t SerializeVersion(uint32_t currentVersion) override;

		void Field(const char* name, bool& value) override;
		void Field(const char* name, int32_t& value) override;
		void Field(const char* name, uint32_t& value) override;
		void Field(const char* name, int64_t& value) override;
		void Field(const char* name, float& value) override;
		void Field(const char* name, double& value) override;
		void Field(const char* name, std::string& value) override;
		void Field(const char* name, Uid& value) override;

		void BeginObject(const char*) override {}
		void EndObject() override {}

	private:
		void WriteBytes(const void* data, size_t size);

	private:
		std::vector<std::byte>& Buffer;
	};

	class FLEX_API BinaryReader final : public Archive
	{
	public:
		explicit BinaryReader(std::span<const std::byte> data);

		using Archive::Field; // keep the math overloads visible next to the overrides

		uint32_t SerializeVersion(uint32_t currentVersion) override;

		void Field(const char* name, bool& value) override;
		void Field(const char* name, int32_t& value) override;
		void Field(const char* name, uint32_t& value) override;
		void Field(const char* name, int64_t& value) override;
		void Field(const char* name, float& value) override;
		void Field(const char* name, double& value) override;
		void Field(const char* name, std::string& value) override;
		void Field(const char* name, Uid& value) override;

		void BeginObject(const char*) override {}
		void EndObject() override {}

		// True once a read ran past the end of the data; every later read is skipped.
		bool HasFailed() const { return bFailed; }

	private:
		bool ReadBytes(void* outData, size_t size, const char* name);

	private:
		std::span<const std::byte> Data;
		size_t Offset = 0;
		bool bFailed = false;
	};
}
