#pragma once
#include <SDL3/SDL_dialog.h>
#include <filesystem>
#include <mutex>
#include <string>

namespace Flex
{
	// Wraps SDL3's native file dialogs. SDL may call back from another thread,
	// so the result is stored behind a mutex and picked up on the main thread
	// with PollResult() once per frame.
	//
	// The object must stay alive while a dialog is open.
	class FileDialog
	{
	public:
		FileDialog() = default;
		FileDialog(const FileDialog&) = delete;
		FileDialog& operator=(const FileDialog&) = delete;

		// filterPattern is a ';'-separated list of extensions without dots, e.g. "flexproj".
		void OpenFile(SDL_Window* window, const std::string& filterName, const std::string& filterPattern, const std::filesystem::path& defaultLocation = {});
		void OpenFolder(SDL_Window* window, const std::filesystem::path& defaultLocation = {});

		bool IsOpen() const;

		// Returns true once after the user picked something; the choice is written to outPath.
		bool PollResult(std::filesystem::path& outPath);

	private:
		static void SDLCALL OnDialogClosed(void* userdata, const char* const* filelist, int filter);

	private:
		mutable std::mutex Lock;
		bool bIsOpen = false;
		bool bHasResult = false;
		std::filesystem::path Result;

		// SDL keeps pointers to these until the dialog closes.
		std::string FilterName;
		std::string FilterPattern;
		std::string DefaultLocation;
		SDL_DialogFileFilter Filter{};
	};
}
