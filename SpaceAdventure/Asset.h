#pragma once
#include <string>
#include <filesystem>
#include <ostream>
#include <istream>
#include <vector>
#include "AssetRegistery.h"

class Asset
{
public:
	virtual ~Asset() = default;

	virtual const std::string GetType() { return "asset"; }
	virtual const std::vector<std::string> GetExtentions() { return std::vector<std::string>(); }

	virtual void Load(std::ifstream& inputstream) {}
	virtual void Save(std::ofstream& outputstream) {}
	virtual void ImportFrom(std::string filepath) {}
	

	void MoveLocation(std::string filepath);

	const std::string& GetName() const { return Name; }
	const std::string& GetPath() const { return FilePath; }
	const std::string& GetSource() const { return Source; }
private:
	std::string Name;
	std::string FilePath;
	std::string Source; // Represents imported files like .png or .mp3 or .wav
};