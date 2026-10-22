#include "Core/Serialization/TextDocument.h"
#include "Core/FileSystem.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace Flex
{
	namespace
	{
		std::string_view Trim(std::string_view text)
		{
			const auto first = text.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos) {
				return {};
			}
			const auto last = text.find_last_not_of(" \t\r\n");
			return text.substr(first, last - first + 1);
		}

		// Splits "value ; comment" on the first ';' that isn't inside a "string".
		void SplitComment(std::string_view line, std::string_view& outContent, std::string_view& outComment)
		{
			bool bInString = false;
			for (size_t index{ 0 }; index < line.size(); ++index)
			{
				const char c = line[index];
				if (bInString && c == '\\') {
					++index; // skip the escaped character
				}
				else if (c == '"') {
					bInString = !bInString;
				}
				else if (c == ';' && !bInString) {
					outContent = Trim(line.substr(0, index));
					outComment = Trim(line.substr(index + 1));
					return;
				}
			}
			outContent = Trim(line);
			outComment = {};
		}

		// Reads the next token from a section header: a word, or a "quoted string" (quotes kept).
		std::string_view NextHeaderToken(std::string_view& text)
		{
			text = Trim(text);
			if (text.empty()) {
				return {};
			}

			size_t end = 0;
			bool bInString = false;
			for (; end < text.size(); ++end)
			{
				const char c = text[end];
				if (bInString && c == '\\') {
					++end;
				}
				else if (c == '"') {
					bInString = !bInString;
				}
				else if (!bInString && (c == ' ' || c == '\t')) {
					break;
				}
			}
			end = std::min(end, text.size());
			const std::string_view token = text.substr(0, end);
			text.remove_prefix(end);
			return token;
		}

		bool IsValidName(std::string_view name)
		{
			if (name.empty()) {
				return false;
			}
			for (char c : name) {
				if (c == ' ' || c == '\t' || c == '=' || c == '[' || c == ']' || c == '"' || c == ';') {
					return false;
				}
			}
			return true;
		}
	}

	// -----------------------
	// TextSection
	// -----------------------

	const std::string* TextSection::GetAttribute(std::string_view name) const
	{
		for (const auto& [key, value] : Attributes) {
			if (key == name) {
				return &value;
			}
		}
		return nullptr;
	}

	void TextSection::SetAttribute(std::string_view name, std::string value)
	{
		for (auto& [key, existing] : Attributes) {
			if (key == name) {
				existing = std::move(value);
				return;
			}
		}
		Attributes.emplace_back(std::string(name), std::move(value));
	}

	const TextProperty* TextSection::Find(std::string_view key) const
	{
		for (const TextProperty& property : Properties) {
			if (property.Key == key) {
				return &property;
			}
		}
		return nullptr;
	}

	TextProperty* TextSection::Find(std::string_view key)
	{
		for (TextProperty& property : Properties) {
			if (property.Key == key) {
				return &property;
			}
		}
		return nullptr;
	}

	const std::string* TextSection::GetValue(std::string_view key) const
	{
		const TextProperty* property = Find(key);
		return property ? &property->Value : nullptr;
	}

	void TextSection::SetValue(std::string_view key, std::string value, std::string comment)
	{
		if (TextProperty* property = Find(key)) {
			property->Value = std::move(value);
			if (!comment.empty()) {
				property->Comment = std::move(comment);
			}
			return;
		}
		Properties.push_back(TextProperty{ std::string(key), std::move(value), std::move(comment), {} });
	}

	bool TextSection::Remove(std::string_view key)
	{
		return std::erase_if(Properties, [key](const TextProperty& property) { return property.Key == key; }) > 0;
	}

	// -----------------------
	// TextDocument
	// -----------------------

	bool TextDocument::Parse(std::string_view text, std::string& outError)
	{
		Sections.clear();
		TrailingComments.clear();

		// Skip a UTF-8 byte order mark if an editor added one.
		if (text.starts_with("\xEF\xBB\xBF")) {
			text.remove_prefix(3);
		}

		std::vector<std::string> pendingComments;
		size_t lineNumber = 0;

		while (!text.empty())
		{
			const size_t lineEnd = text.find('\n');
			const std::string_view rawLine = text.substr(0, lineEnd);
			text.remove_prefix(lineEnd == std::string_view::npos ? text.size() : lineEnd + 1);
			++lineNumber;

			const std::string_view line = Trim(rawLine);
			if (line.empty()) {
				continue;
			}

			if (line.front() == ';') {
				pendingComments.emplace_back(Trim(line.substr(1)));
				continue;
			}

			std::string_view content;
			std::string_view comment;
			SplitComment(line, content, comment);

			if (content.front() == '[')
			{
				if (content.back() != ']') {
					outError = "Line " + std::to_string(lineNumber) + ": section header is missing ']'.";
					return false;
				}

				std::string_view header = content.substr(1, content.size() - 2);
				const std::string_view type = NextHeaderToken(header);
				if (!IsValidName(type)) {
					outError = "Line " + std::to_string(lineNumber) + ": invalid section type.";
					return false;
				}

				TextSection& section = AddSection(std::string(type));
				section.LeadingComments = std::move(pendingComments);
				pendingComments.clear();

				for (std::string_view token = NextHeaderToken(header); !token.empty(); token = NextHeaderToken(header))
				{
					const size_t separator = token.find('=');
					if (separator == std::string_view::npos || !IsValidName(token.substr(0, separator))) {
						outError = "Line " + std::to_string(lineNumber) + ": invalid attribute '" + std::string(token) + "'.";
						return false;
					}
					section.SetAttribute(token.substr(0, separator), std::string(token.substr(separator + 1)));
				}
				continue;
			}

			const size_t separator = content.find('=');
			if (separator == std::string_view::npos) {
				outError = "Line " + std::to_string(lineNumber) + ": expected 'Key = Value'.";
				return false;
			}
			if (Sections.empty()) {
				outError = "Line " + std::to_string(lineNumber) + ": property before the first [Section].";
				return false;
			}

			const std::string_view key = Trim(content.substr(0, separator));
			if (!IsValidName(key)) {
				outError = "Line " + std::to_string(lineNumber) + ": invalid key '" + std::string(key) + "'.";
				return false;
			}

			TextProperty property{ std::string(key), std::string(Trim(content.substr(separator + 1))), std::string(comment), std::move(pendingComments) };
			pendingComments.clear();
			Sections.back().Properties.push_back(std::move(property));
		}

		TrailingComments = std::move(pendingComments);
		return true;
	}

	std::string TextDocument::ToString() const
	{
		std::ostringstream output;
		auto writeComments = [&output](const std::vector<std::string>& comments) {
			for (const std::string& comment : comments) {
				output << "; " << comment << '\n';
			}
		};

		bool bFirstSection = true;
		for (const TextSection& section : Sections)
		{
			if (!bFirstSection) {
				output << '\n';
			}
			bFirstSection = false;

			writeComments(section.LeadingComments);
			output << '[' << section.Type;
			for (const auto& [name, value] : section.Attributes) {
				output << ' ' << name << '=' << value;
			}
			output << "]\n";

			for (const TextProperty& property : section.Properties)
			{
				writeComments(property.LeadingComments);
				output << property.Key << " = " << property.Value;
				if (!property.Comment.empty()) {
					output << "  ; " << property.Comment;
				}
				output << '\n';
			}
		}

		if (!TrailingComments.empty()) {
			output << '\n';
			writeComments(TrailingComments);
		}
		return output.str();
	}

	bool TextDocument::Load(const std::filesystem::path& filePath, std::string& outError)
	{
		std::ifstream inputstream(filePath, std::ios::binary);
		if (!inputstream.is_open()) {
			outError = "Could not open " + PathToUtf8(filePath);
			return false;
		}

		std::ostringstream content;
		content << inputstream.rdbuf();

		if (!Parse(content.str(), outError)) {
			outError = PathToUtf8(filePath) + ": " + outError;
			return false;
		}
		return true;
	}

	bool TextDocument::Save(const std::filesystem::path& filePath, std::string& outError) const
	{
		// Binary mode: always '\n' line endings, the same on every platform.
		std::ofstream outputstream(filePath, std::ios::binary | std::ios::trunc);
		if (!outputstream.is_open()) {
			outError = "Could not write " + PathToUtf8(filePath);
			return false;
		}

		outputstream << ToString();
		if (!outputstream.good()) {
			outError = "Error while writing " + PathToUtf8(filePath);
			return false;
		}
		return true;
	}

	TextSection& TextDocument::AddSection(std::string type)
	{
		TextSection& section = Sections.emplace_back();
		section.Type = std::move(type);
		return section;
	}

	TextSection* TextDocument::FindSection(std::string_view type)
	{
		for (TextSection& section : Sections) {
			if (section.Type == type) {
				return &section;
			}
		}
		return nullptr;
	}

	const TextSection* TextDocument::FindSection(std::string_view type) const
	{
		for (const TextSection& section : Sections) {
			if (section.Type == type) {
				return &section;
			}
		}
		return nullptr;
	}

	TextSection& TextDocument::FindOrAddSection(std::string_view type)
	{
		if (TextSection* section = FindSection(type)) {
			return *section;
		}
		return AddSection(std::string(type));
	}
}
