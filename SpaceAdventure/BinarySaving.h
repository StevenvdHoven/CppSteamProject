#pragma once
#include "Math.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>

class BinarySaving
{
public:
	static void Write(std::ofstream& outstream, Math::Vector2 vec2);
	static void Write(std::ofstream& outstream, const std::string& string);
	static Math::Vector2 ReadVector2(std::ifstream& inputstream);
	static std::string ReadString(std::ifstream& inputstream);

	static void ReadAsset(std::ifstream& inputstream, std::string& assetType, std::string& source);
};

