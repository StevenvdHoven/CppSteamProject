#include "Editor/FileDialog.h"
#include "Core/FileSystem.h"
#include <SDL3/SDL.h>

namespace Flex
{
	void FileDialog::OpenFile(SDL_Window* window, const std::string& filterName, const std::string& filterPattern, const std::filesystem::path& defaultLocation)
	{
		{
			std::lock_guard<std::mutex> lock(Lock);
			if (bIsOpen) {
				return;
			}
			bIsOpen = true;
			bHasResult = false;
		}

		FilterName = filterName;
		FilterPattern = filterPattern;
		DefaultLocation = PathToUtf8(defaultLocation);
		Filter = SDL_DialogFileFilter{ FilterName.c_str(), FilterPattern.c_str() };

		SDL_ShowOpenFileDialog(&FileDialog::OnDialogClosed, this, window, &Filter, 1, DefaultLocation.empty() ? nullptr : DefaultLocation.c_str(), false);
	}

	void FileDialog::OpenFolder(SDL_Window* window, const std::filesystem::path& defaultLocation)
	{
		{
			std::lock_guard<std::mutex> lock(Lock);
			if (bIsOpen) {
				return;
			}
			bIsOpen = true;
			bHasResult = false;
		}

		DefaultLocation = PathToUtf8(defaultLocation);
		SDL_ShowOpenFolderDialog(&FileDialog::OnDialogClosed, this, window, DefaultLocation.empty() ? nullptr : DefaultLocation.c_str(), false);
	}

	bool FileDialog::IsOpen() const
	{
		std::lock_guard<std::mutex> lock(Lock);
		return bIsOpen;
	}

	bool FileDialog::PollResult(std::filesystem::path& outPath)
	{
		std::lock_guard<std::mutex> lock(Lock);
		if (!bHasResult) {
			return false;
		}
		bHasResult = false;
		outPath = Result;
		return true;
	}

	void SDLCALL FileDialog::OnDialogClosed(void* userdata, const char* const* filelist, int)
	{
		FileDialog* dialog = static_cast<FileDialog*>(userdata);
		std::lock_guard<std::mutex> lock(dialog->Lock);
		dialog->bIsOpen = false;

		// filelist is NULL on error, and empty when the user cancelled.
		if (!filelist) {
			SDL_Log("File dialog failed: %s", SDL_GetError());
			return;
		}
		if (!filelist[0]) {
			return;
		}

		dialog->Result = Utf8ToPath(filelist[0]);
		dialog->bHasResult = true;
	}
}
