#pragma once
#include <string>
#include <filesystem>
#include <ostream>
#include <istream>
#include <vector>
#include "AssetRegistery.h"

class AssetManager;
class SceneManager;
class ContentWindow;

class Asset
{
public:
	virtual ~Asset() = default;

	virtual const std::string GetType() { return "asset"; }
	virtual const std::vector<std::string> GetExtentions() { return std::vector<std::string>(); }

	virtual void Load(std::ifstream&) {}
	virtual void Save(std::ofstream&) {}
	virtual void ImportFrom(const std::string&) {}

#ifdef _DEBUG
	virtual void ImGui_ContentHelper();
#endif

	void MoveLocation(std::string filepath);

	const std::string& GetName() const { return Name; }
	const std::string& GetPath() const { return FilePath; }
	const std::string& GetSource() const { return Source; }
private:
	std::string Name;
	std::string FilePath;
	std::string Source; // Represents imported files like .png or .mp3 or .wav

	friend AssetManager;
	friend SceneManager;
	friend ContentWindow;
};