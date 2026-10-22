#include "Core/Serialization/TextArchive.h"
#include "Core/Serialization/TextValue.h"
#include <limits>

namespace Flex
{
	TextArchive::TextArchive(TextSection& section, ArchiveMode mode) :
		Archive(mode),
		Section{ section }
	{
	}

	uint32_t TextArchive::SerializeVersion(uint32_t currentVersion)
	{
		if (IsSaving()) {
			Section.SetAttribute("version", TextValue::FormatInt(currentVersion));
			return currentVersion;
		}

		const std::string* value = Section.GetAttribute("version");
		int64_t version = 0;
		if (value && (!TextValue::ParseInt(*value, version) || version < 0)) {
			WarnUnreadable("version", *value, "a positive whole number");
			version = 0;
		}
		return static_cast<uint32_t>(version);
	}

	std::string TextArchive::MakeKey(const char* name) const
	{
		return KeyPrefix + name;
	}

	const std::string* TextArchive::GetValueForLoad(const char* name, std::string& outKey)
	{
		outKey = MakeKey(name);
		return Section.GetValue(outKey);
	}

	void TextArchive::WarnUnreadable(const std::string& key, const std::string& value, const char* expected)
	{
		AddWarning("[" + Section.Type + "] " + key + " = " + value + ": expected " + expected + ", kept the default.");
	}

	template<typename T>
	void TextArchive::IntegerField(const char* name, T& value)
	{
		if (IsSaving()) {
			Section.SetValue(MakeKey(name), TextValue::FormatInt(static_cast<int64_t>(value)));
			return;
		}

		std::string key;
		if (const std::string* text = GetValueForLoad(name, key)) {
			int64_t parsed = 0;
			if (TextValue::ParseInt(*text, parsed) && parsed >= static_cast<int64_t>(std::numeric_limits<T>::min()) && parsed <= static_cast<int64_t>(std::numeric_limits<T>::max())) {
				value = static_cast<T>(parsed);
			}
			else {
				WarnUnreadable(key, *text, "a whole number in range");
			}
		}
	}

	void TextArchive::Field(const char* name, bool& value)
	{
		if (IsSaving()) {
			Section.SetValue(MakeKey(name), TextValue::FormatBool(value));
			return;
		}

		std::string key;
		if (const std::string* text = GetValueForLoad(name, key)) {
			if (!TextValue::ParseBool(*text, value)) {
				WarnUnreadable(key, *text, "true or false");
			}
		}
	}

	void TextArchive::Field(const char* name, int32_t& value)
	{
		IntegerField(name, value);
	}

	void TextArchive::Field(const char* name, uint32_t& value)
	{
		IntegerField(name, value);
	}

	void TextArchive::Field(const char* name, int64_t& value)
	{
		IntegerField(name, value);
	}

	void TextArchive::Field(const char* name, float& value)
	{
		if (IsSaving()) {
			Section.SetValue(MakeKey(name), TextValue::FormatFloat(value));
			return;
		}

		std::string key;
		if (const std::string* text = GetValueForLoad(name, key)) {
			if (!TextValue::ParseFloat(*text, value)) {
				WarnUnreadable(key, *text, "a number");
			}
		}
	}

	void TextArchive::Field(const char* name, double& value)
	{
		if (IsSaving()) {
			Section.SetValue(MakeKey(name), TextValue::FormatDouble(value));
			return;
		}

		std::string key;
		if (const std::string* text = GetValueForLoad(name, key)) {
			if (!TextValue::ParseDouble(*text, value)) {
				WarnUnreadable(key, *text, "a number");
			}
		}
	}

	void TextArchive::Field(const char* name, std::string& value)
	{
		if (IsSaving()) {
			Section.SetValue(MakeKey(name), TextValue::FormatString(value));
			return;
		}

		std::string key;
		if (const std::string* text = GetValueForLoad(name, key)) {
			if (!TextValue::ParseString(*text, value)) {
				WarnUnreadable(key, *text, "a \"string\"");
			}
		}
	}

	void TextArchive::Field(const char* name, Uid& value)
	{
		if (IsSaving()) {
			Section.SetValue(MakeKey(name), TextValue::FormatUid(value));
			return;
		}

		std::string key;
		if (const std::string* text = GetValueForLoad(name, key)) {
			if (!TextValue::ParseUid(*text, value)) {
				WarnUnreadable(key, *text, "uid(<16 hex characters>)");
			}
		}
	}

	void TextArchive::BeginObject(const char* name)
	{
		PrefixLengths.push_back(KeyPrefix.size());
		KeyPrefix += name;
		KeyPrefix += '.';
	}

	void TextArchive::EndObject()
	{
		if (PrefixLengths.empty()) {
			return;
		}
		KeyPrefix.resize(PrefixLengths.back());
		PrefixLengths.pop_back();
	}
}
