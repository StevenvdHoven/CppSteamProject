#include "Editor/OutputLog.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>

namespace Flex
{
	namespace
	{
		SDL_LogOutputFunction DefaultLogOutput = nullptr;
		void* DefaultLogUserData = nullptr;

		void SDLCALL LogToOutput(void* userData, int category, SDL_LogPriority priority, const char* message)
		{
			if (DefaultLogOutput) {
				DefaultLogOutput(DefaultLogUserData, category, priority, message);
			}
			OutputLog* log = static_cast<OutputLog*>(userData);
			const OutputLog::Level level = priority >= SDL_LOG_PRIORITY_ERROR ? OutputLog::Level::Error
				: priority == SDL_LOG_PRIORITY_WARN ? OutputLog::Level::Warning : OutputLog::Level::Info;
			log->Add(level, message);
		}

		bool ContainsWord(const std::string& lowerText, const char* word)
		{
			return lowerText.find(word) != std::string::npos;
		}
	}

	void OutputLog::Add(Level level, std::string text)
	{
		while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) {
			text.pop_back();
		}
		std::scoped_lock lock{ Mutex };
		Lines.push_back({ level, std::move(text) });
		if (Lines.size() > MaxLines) {
			Lines.erase(Lines.begin(), Lines.begin() + static_cast<std::ptrdiff_t>(Lines.size() - MaxLines));
		}
		++Revision;
	}

	void OutputLog::AddDetected(std::string text)
	{
		std::string lower = text;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		Level level = Level::Info;
		if (ContainsWord(lower, ": error") || ContainsWord(lower, " error ") || ContainsWord(lower, "error:") || ContainsWord(lower, "fatal error")) {
			level = Level::Error;
		} else if (ContainsWord(lower, ": warning") || ContainsWord(lower, " warning ")) {
			level = Level::Warning;
		}
		Add(level, std::move(text));
	}

	void OutputLog::Clear()
	{
		std::scoped_lock lock{ Mutex };
		Lines.clear();
		++Revision;
	}

	void OutputLog::Read(const std::function<void(const std::vector<Line>&)>& visit) const
	{
		std::scoped_lock lock{ Mutex };
		visit(Lines);
	}

	uint64_t OutputLog::GetRevision() const
	{
		std::scoped_lock lock{ Mutex };
		return Revision;
	}

	void OutputLog::InstallSdlLogHook()
	{
		SDL_GetLogOutputFunction(&DefaultLogOutput, &DefaultLogUserData);
		SDL_SetLogOutputFunction(&LogToOutput, this);
	}

	void OutputLog::UninstallSdlLogHook()
	{
		if (DefaultLogOutput) {
			SDL_SetLogOutputFunction(DefaultLogOutput, DefaultLogUserData);
		} else {
			SDL_SetLogOutputFunction(SDL_GetDefaultLogOutputFunction(), nullptr);
		}
	}
}
