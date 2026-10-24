#pragma once
#include "Editor/EditorCore.h"
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace Flex
{
	// Helpers for running external build tools from the editor.
	namespace BuildTools
	{
		// The engine folder (FlexEngine/), found from the running exe in Binaries/<Config>/.
		FLEX_EDITOR_API std::filesystem::path GetEngineRoot();

		// MSBuild.exe of the newest Visual Studio, found with vswhere (cached). Empty if none.
		FLEX_EDITOR_API std::filesystem::path FindMSBuild(std::string& outError);

		// Runs a program and waits for it. Every output line (stdout and stderr) goes to onLine.
		// Returns the exit code, or -1 when it couldn't start.
		FLEX_EDITOR_API int RunProcess(const std::vector<std::string>& arguments, const std::function<void(const std::string&)>& onLine);
	}

	// One piece of work on a background thread (a build). The editor polls it every frame.
	class FLEX_EDITOR_API BackgroundJob
	{
	public:
		~BackgroundJob();

		// False when a job is still running.
		bool Start(std::function<bool()> work);
		bool IsRunning() const { return bRunning; }

		// True once, on the main thread, after the job finished; outSucceeded is what work returned.
		bool Poll(bool& outSucceeded);

	private:
		std::thread Thread;
		std::atomic<bool> bDone = false;
		std::atomic<bool> bSucceeded = false;
		bool bRunning = false;
	};
}
