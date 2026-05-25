#include "BinarySaving.h"
#include <string>
#include <fstream>
#include <filesystem>

void BinarySaving::Write(std::ofstream& outstream, Math::Vector2 vec2)
{
}

void BinarySaving::Write(std::ofstream& outstream, const std::string& string)
{
	std::uint32_t stringSize = static_cast<std::uint32_t>(string.size());
	if (stringSize > 0)
	{
		outstream.write(reinterpret_cast<const char*>(&stringSize), sizeof(stringSize));
		outstream.write(string.data(), stringSize);
	}
}

std::string BinarySaving::ReadString(std::ifstream& inputstream)
{
	std::uint32_t stringsize = 0;
	inputstream.read(reinterpret_cast<char*>(&stringsize), sizeof(stringsize));
	if (stringsize <= 0)
	{
		return "";
	}

	std::string loadString(stringsize, '\0');
	inputstream.read(loadString.data(), stringsize);
	return loadString;
}

void BinarySaving::ReadAsset(std::ifstream& inputstream,std::string& assetType, std::string& source)
{
	assetType = ReadString(inputstream);
	source = ReadString(inputstream);
}
