#pragma once
#include "Core/Core.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Flex
{
	// The FlexEngine text format, used for every file the editor writes
	// (.flexproj, .meta, and later scenes/prefabs). Line based so diffs and merges stay clean:
	//
	//     ; full-line comment
	//     [Entity id=1 name="Player"]       <- section: a type plus optional attributes
	//     Transform.Position = (120, 64)    <- property: key = value
	//     Sprite.Texture = uid(a41f...)     ; trailing comment (e.g. a path hint)
	//
	// Values are kept as raw text here; TextValue / TextArchive turn them into real types.
	// Comments are kept when a file is loaded and saved again. Blank lines are not.

	struct TextProperty
	{
		std::string Key;
		std::string Value;
		std::string Comment;
		std::vector<std::string> LeadingComments;
	};

	class FLEX_API TextSection
	{
	public:
		const std::string* GetAttribute(std::string_view name) const;
		void SetAttribute(std::string_view name, std::string value);

		const TextProperty* Find(std::string_view key) const;
		TextProperty* Find(std::string_view key);
		const std::string* GetValue(std::string_view key) const;

		// Keeps an existing comment unless a new one is given.
		void SetValue(std::string_view key, std::string value, std::string comment = {});
		bool Remove(std::string_view key);

	public:
		std::string Type;
		std::vector<std::pair<std::string, std::string>> Attributes;
		std::vector<TextProperty> Properties;
		std::vector<std::string> LeadingComments;
	};

	class FLEX_API TextDocument
	{
	public:
		bool Parse(std::string_view text, std::string& outError);
		std::string ToString() const;

		bool Load(const std::filesystem::path& filePath, std::string& outError);
		bool Save(const std::filesystem::path& filePath, std::string& outError) const;

		TextSection& AddSection(std::string type);
		TextSection* FindSection(std::string_view type);
		const TextSection* FindSection(std::string_view type) const;
		TextSection& FindOrAddSection(std::string_view type);

	public:
		std::vector<TextSection> Sections;
		std::vector<std::string> TrailingComments;
	};
}
