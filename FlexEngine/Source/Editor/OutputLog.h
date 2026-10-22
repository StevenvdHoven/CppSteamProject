#pragma once
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace Flex
{
	// Lines for the Output window: build output (MSBuild) and everything the engine logs with
	// SDL_Log. Thread-safe: background jobs add lines while the window draws them.
	class OutputLog
	{
	public:
		enum class Level : uint8_t
		{
			Info,
			Warning,
			Error,
			Success
		};

		struct Line
		{
			Level LineLevel = Level::Info;
			std::string Text;
		};

		static constexpr size_t MaxLines = 20000;

		void Add(Level level, std::string text);
		// Picks the level from the text: MSBuild's "error"/"warning" lines are coloured.
		void AddDetected(std::string text);
		void Clear();

		// Calls visit with a locked copy-free view of the lines.
		void Read(const std::function<void(const std::vector<Line>&)>& visit) const;
		uint64_t GetRevision() const;

		// Routes SDL_Log into this log too (and still to the default output). Call Uninstall
		// before the log is destroyed.
		void InstallSdlLogHook();
		void UninstallSdlLogHook();

	private:
		mutable std::mutex Mutex;
		std::vector<Line> Lines;
		uint64_t Revision = 0;
	};
}
