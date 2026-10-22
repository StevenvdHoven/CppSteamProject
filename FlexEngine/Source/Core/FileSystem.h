#pragma once
#include <filesystem>
#include <string>

namespace Flex
{
	// All strings in FlexEngine are UTF-8. std::filesystem::path on Windows is
	// UTF-16 internally, so convert explicitly instead of relying on the ANSI codepage.
	inline std::filesystem::path Utf8ToPath(const std::string& utf8)
	{
		return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
	}

	inline std::string PathToUtf8(const std::filesystem::path& path)
	{
		const std::u8string utf8 = path.u8string();
		return std::string(utf8.begin(), utf8.end());
	}

	// With '/' separators on every platform. Use this when writing paths into files.
	inline std::string PathToGenericUtf8(const std::filesystem::path& path)
	{
		const std::u8string utf8 = path.generic_u8string();
		return std::string(utf8.begin(), utf8.end());
	}
}
