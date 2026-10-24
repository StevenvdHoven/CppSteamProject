#pragma once
// Internal to FlexSteam.dll: how the session drives the lobby. Not for game code.
#include "Plugins/Steam/Runtime/SteamTypes.h"

namespace Flex::Steam::Detail
{
	void LobbyStartup();          // after SteamAPI_Init: creates the lobby's Steam callbacks
	void LobbyShutdown();         // before SteamAPI_Shutdown: leaves the lobby, drops the callbacks
	void LobbyBeginUpdate();      // before SteamAPI_RunCallbacks: clears last frame's events
	void JoinFromCommandLine();   // +connect_lobby <id> (the game was started from an invite)

	void NetworkStartup();        // after SteamAPI_Init: relay access, connection callbacks, hot-reload listener
	void NetworkShutdown();       // before SteamAPI_Shutdown
	void NetworkStart(bool bIsHost, SteamId self, SteamId host); // entered a lobby
	void NetworkStop();           // left the lobby (any way): thread stopped, connections closed, queues cleared
	void NetworkUpdate();         // after SteamAPI_RunCallbacks: the host accepts or refuses connecting users
	void NetworkDisconnect(SteamId user, const char* reason); // host: closes a client's connection
}
