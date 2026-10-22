#include "Core/Serialization/TextValue.h"
#include <charconv>

namespace Flex::TextValue
{
	namespace
	{
		template<typename T>
		bool ParseNumber(std::string_view text, T& outValue)
		{
			if (!text.empty() && text.front() == '+') {
				text.remove_prefix(1);
			}

			T value{};
			const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
			if (text.empty() || error != std::errc{} || end != text.data() + text.size()) {
				return false;
			}
			outValue = value;
			return true;
		}

		template<typename T>
		std::string FormatNumber(T value)
		{
			char buffer[64]{};
			const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
			(void)error;
			return std::string(buffer, end);
		}
	}

	std::string FormatBool(bool value)
	{
		return value ? "true" : "false";
	}

	bool ParseBool(std::string_view text, bool& outValue)
	{
		if (text == "true") {
			outValue = true;
			return true;
		}
		if (text == "false") {
			outValue = false;
			return true;
		}
		return false;
	}

	std::string FormatInt(int64_t value)
	{
		return FormatNumber(value);
	}

	bool ParseInt(std::string_view text, int64_t& outValue)
	{
		return ParseNumber(text, outValue);
	}

	std::string FormatFloat(float value)
	{
		return FormatNumber(value);
	}

	std::string FormatDouble(double value)
	{
		return FormatNumber(value);
	}

	bool ParseFloat(std::string_view text, float& outValue)
	{
		return ParseNumber(text, outValue);
	}

	bool ParseDouble(std::string_view text, double& outValue)
	{
		return ParseNumber(text, outValue);
	}

	std::string FormatString(std::string_view value)
	{
		std::string result;
		result.reserve(value.size() + 2);
		result += '"';
		for (char c : value)
		{
			switch (c)
			{
			case '"': result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\n': result += "\\n"; break;
			case '\t': result += "\\t"; break;
			case '\r': break;
			default: result += c; break;
			}
		}
		result += '"';
		return result;
	}

	bool ParseString(std::string_view text, std::string& outValue)
	{
		if (text.size() < 2 || text.front() != '"' || text.back() != '"') {
			// Unquoted: older files (and hand-edited ones) store plain text.
			outValue = std::string(text);
			return true;
		}

		std::string result;
		result.reserve(text.size());
		for (size_t index{ 1 }; index + 1 < text.size(); ++index)
		{
			const char c = text[index];
			if (c != '\\') {
				result += c;
				continue;
			}

			if (index + 2 >= text.size()) {
				return false; // backslash right before the closing quote
			}
			const char escaped = text[++index];
			switch (escaped)
			{
			case '"': result += '"'; break;
			case '\\': result += '\\'; break;
			case 'n': result += '\n'; break;
			case 't': result += '\t'; break;
			default: return false;
			}
		}
		outValue = std::move(result);
		return true;
	}

	std::string FormatUid(Uid value)
	{
		return "uid(" + value.ToString() + ")";
	}

	bool ParseUid(std::string_view text, Uid& outValue)
	{
		constexpr std::string_view PREFIX = "uid(";
		if (text.starts_with(PREFIX) && text.ends_with(')')) {
			text = text.substr(PREFIX.size(), text.size() - PREFIX.size() - 1);
		}

		if (const auto uid = Uid::FromString(text)) {
			outValue = *uid;
			return true;
		}
		return false;
	}
}
