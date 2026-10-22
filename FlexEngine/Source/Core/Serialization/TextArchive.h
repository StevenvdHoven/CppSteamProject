#pragma once
#include "Core/Serialization/Archive.h"
#include "Core/Serialization/TextDocument.h"

namespace Flex
{
	// Reads or writes the fields of one TextSection. Fields are stored by name, so
	// field order doesn't matter, unknown keys are kept, and missing keys keep their defaults.
	// The version is stored as the section attribute "version".
	class FLEX_API TextArchive final : public Archive
	{
	public:
		TextArchive(TextSection& section, ArchiveMode mode);

		uint32_t SerializeVersion(uint32_t currentVersion) override;

		void Field(const char* name, bool& value) override;
		void Field(const char* name, int32_t& value) override;
		void Field(const char* name, uint32_t& value) override;
		void Field(const char* name, int64_t& value) override;
		void Field(const char* name, float& value) override;
		void Field(const char* name, double& value) override;
		void Field(const char* name, std::string& value) override;
		void Field(const char* name, Uid& value) override;

		void BeginObject(const char* name) override;
		void EndObject() override;

	private:
		std::string MakeKey(const char* name) const;
		const std::string* GetValueForLoad(const char* name, std::string& outKey);
		void WarnUnreadable(const std::string& key, const std::string& value, const char* expected);

		template<typename T>
		void IntegerField(const char* name, T& value);

	private:
		TextSection& Section;
		std::string KeyPrefix;
		std::vector<size_t> PrefixLengths;
	};
}
