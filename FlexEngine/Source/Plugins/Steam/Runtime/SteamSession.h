#pragma once
#include "Plugins/Steam/Runtime/SteamApi.h"
#include "Plugins/Steam/Runtime/SteamTypes.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Flex::Steam
{
	struct Friend
	{
		SteamId Id;
		std::string Name;
		bool bIsOnline = false;
		bool bIsPlayingThisGame = false;
	};

	// The connection to the Steam client (main thread only). Everything else in the plugin needs it.
	//
	// It starts by itself: the system SteamUpdate (Update phase, order -1000, so before the game's own
	// systems) calls InitFromProject the first time it runs and Update every frame after that. Without a
	// running Steam client every Steam call does nothing and IsInitialized stays false.
	//
	// The app id comes from the project ([Steam version=1] AppId = 480 in the .flexproj; 480 = Spacewar,
	// Valve's test app, while there's none). It's passed to Steam through the SteamAppId environment
	// variable, so no steam_appid.txt is needed next to the editor.
	class FLEX_STEAM_API Session
	{
	public:
		static constexpr uint32_t DevelopmentAppId = 480; // Spacewar

		// Starts Steam once; later calls return the first result. outError: why it failed
		// (Steam isn't running, not logged in, the app isn't owned, ...).
		static bool Init(uint32_t appId, std::string* outError = nullptr);
		// Init with the active project's app id (DevelopmentAppId without a project or setting).
		static bool InitFromProject(std::string* outError = nullptr);
		// Leaves the lobby and shuts Steam down (the editor calls it on exit; a game may leave it to the
		// process exit, Steam cleans up after a closed process).
		static void Shutdown();

		static bool IsInitialized();
		static const std::string& GetInitError();
		static uint32_t GetAppId();

		// Runs Steam's callbacks and the lobby's bookkeeping. Call it once per frame from one place:
		// the SteamUpdate system does it while a game runs.
		static void Update();

		// Whether Update starts Steam by itself (InitFromProject) the first time it runs. On by default;
		// the editor turns it off for projects that don't use Steam, so pressing Play there signs no one in.
		static void SetAutoInit(bool bAutoInit);

		// Whether Shutdown runs by itself when the app quits (SDL_EVENT_QUIT on the thread that called
		// Init), so a game leaves its lobby and closes its connections cleanly. On by default; the editor
		// turns it off (it shuts Steam down itself, and a quit can still be cancelled there).
		static void SetShutdownOnQuit(bool bShutdownOnQuit);

		// Shipping builds: true when the game wasn't started by Steam and Steam is starting it again,
		// so the game should quit at once. Call it before Init.
		static bool RestartThroughSteamIfNeeded(uint32_t appId);

		static SteamId GetLocalUser();
		static std::string GetName(SteamId user); // Steam persona name ("" before Init)

		// Steam friends (for an in-game invite list), online ones first, then by name.
		static std::vector<Friend> GetFriends();
	};
}
