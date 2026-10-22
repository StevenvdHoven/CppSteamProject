#include "Editor/BuildTools.h"
#include "Core/FileSystem.h"
#include <SDL3/SDL.h>
#include <mutex>

namespace Flex
{
	std::filesystem::path BuildTools::GetEngineRoot()
	{
		const char* basePath = SDL_GetBasePath(); // .../FlexEngine/Binaries/<Config>/
		if (!basePath) {
			return {};
		}
		std::error_code error;
		const std::filesystem::path root = std::filesystem::weakly_canonical(Utf8ToPath(basePath) / ".." / "..", error);
		return root;
	}

	std::filesystem::path BuildTools::FindMSBuild(std::string& outError)
	{
		static std::mutex lock;
		static std::filesystem::path cached;
		std::scoped_lock guard{ lock };
		if (!cached.empty()) {
			return cached;
		}

		const char* programFiles = SDL_getenv("ProgramFiles(x86)");
		const std::filesystem::path vswhere = Utf8ToPath(programFiles ? programFiles : "C:\\Program Files (x86)") / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
		std::error_code error;
		if (!std::filesystem::exists(vswhere, error)) {
			outError = "Visual Studio wasn't found (no vswhere.exe). Install Visual Studio 2026 with the C++ workload.";
			return {};
		}
		std::string found;
		RunProcess({ PathToUtf8(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.Component.MSBuild", "-find", "MSBuild\\**\\Bin\\MSBuild.exe" },
			[&](const std::string& line) {
				if (found.empty() && !line.empty()) {
					found = line;
				}
			});
		if (found.empty()) {
			outError = "MSBuild wasn't found. Install Visual Studio 2026 with the C++ workload.";
			return {};
		}
		cached = Utf8ToPath(found);
		return cached;
	}

	int BuildTools::RunProcess(const std::vector<std::string>& arguments, const std::function<void(const std::string&)>& onLine)
	{
		std::vector<const char*> argv;
		for (const std::string& argument : arguments) {
			argv.push_back(argument.c_str());
		}
		argv.push_back(nullptr);

		SDL_PropertiesID properties = SDL_CreateProperties();
		SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
		SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
		SDL_SetBooleanProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
		SDL_Process* process = SDL_CreateProcessWithProperties(properties);
		SDL_DestroyProperties(properties);
		if (!process) {
			onLine(std::string("Could not start ") + arguments.front() + ": " + SDL_GetError());
			return -1;
		}

		// Split the output into lines as it arrives.
		SDL_IOStream* output = SDL_GetProcessOutput(process);
		std::string pending;
		char buffer[4096];
		while (output) {
			const size_t read = SDL_ReadIO(output, buffer, sizeof(buffer));
			if (read == 0) {
				// The stream doesn't block: "not ready" means wait for more, anything else is the end.
				if (SDL_GetIOStatus(output) == SDL_IO_STATUS_NOT_READY) {
					SDL_Delay(5);
					continue;
				}
				break;
			}
			pending.append(buffer, read);
			for (size_t newline = pending.find('\n'); newline != std::string::npos; newline = pending.find('\n')) {
				std::string line = pending.substr(0, newline);
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				onLine(line);
				pending.erase(0, newline + 1);
			}
		}
		if (!pending.empty()) {
			onLine(pending);
		}

		int exitCode = -1;
		SDL_WaitProcess(process, true, &exitCode);
		SDL_DestroyProcess(process);
		return exitCode;
	}

	BackgroundJob::~BackgroundJob()
	{
		if (Thread.joinable()) {
			Thread.join();
		}
	}

	bool BackgroundJob::Start(std::function<bool()> work)
	{
		if (bRunning) {
			return false;
		}
		if (Thread.joinable()) {
			Thread.join();
		}
		bRunning = true;
		bDone = false;
		Thread = std::thread([this, work = std::move(work)]() {
			bSucceeded = work();
			bDone = true;
		});
		return true;
	}

	bool BackgroundJob::Poll(bool& outSucceeded)
	{
		if (!bRunning || !bDone) {
			return false;
		}
		Thread.join();
		bRunning = false;
		outSucceeded = bSucceeded;
		return true;
	}
}
