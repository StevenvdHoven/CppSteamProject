#pragma once
#include "Core/Core.h"
#include "Core/Uid.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace Flex::TextValue
{
	// Formatting and parsing of single values in the FlexEngine text format.
	//
	//     bool     true / false
	//     integer  -12
	//     float    0.1   (shortest text that reads back to exactly the same value)
	//     string   "Hello \"world\""   (escapes: \" \\ \n \t)
	//     uid      uid(a41f09c3d2e85b70)
	//
	// Parse* functions return false and leave the output untouched when the text doesn't fit.

	FLEX_API std::string FormatBool(bool value);
	FLEX_API bool ParseBool(std::string_view text, bool& outValue);

	FLEX_API std::string FormatInt(int64_t value);
	FLEX_API bool ParseInt(std::string_view text, int64_t& outValue);

	FLEX_API std::string FormatFloat(float value);
	FLEX_API std::string FormatDouble(double value);
	FLEX_API bool ParseFloat(std::string_view text, float& outValue);
	FLEX_API bool ParseDouble(std::string_view text, double& outValue);

	FLEX_API std::string FormatString(std::string_view value);
	// Also accepts unquoted text (taken as-is), so older hand-written files keep loading.
	FLEX_API bool ParseString(std::string_view text, std::string& outValue);

	FLEX_API std::string FormatUid(Uid value);
	// Accepts uid(<16 hex>) and a bare <16 hex>.
	FLEX_API bool ParseUid(std::string_view text, Uid& outValue);
}
