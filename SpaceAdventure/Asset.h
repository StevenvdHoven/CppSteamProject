#pragma once
#include <string>
#include <filesystem>
#include <ostream>
#include <istream>
#include "AssetRegistery.h"

class Asset
{
public:
	Asset() = default;

	virtual void Load(std::ifstream& inputstream) = 0;
	virtual void Save(std::ofstream& outputstream) = 0;

	void MoveLocation(std::string filepath);

	const std::string& GetName() const { return Name; }
	const std::string& GetPath() const { return FilePath; }

private:
	std::string Name;
	std::string FilePath;
	std::string Source; // Represents imported files like .png or .mp3 or .wav
};