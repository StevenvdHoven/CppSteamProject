#pragma once
// Internal to FlexSteam.dll: how the session drives the lobby. Not for game code.
#include "Plugins/Steam/Runtime/SteamTypes.h"

namespace Flex::Steam::Detail
{
	void LobbyStartup();          // after SteamAPI_Init: creates the lobby's Steam callbacks
	void LobbyShutdown();         // before SteamAPI_Shutdown: leaves the lobby, drops the callbacks
	void LobbyBeginUpdate();      // before SteamAPI_RunCallbacks: clears last frame's events
	void JoinFromCommandLine();   // +connect_lobby <id> (the game was started from an invite)
}
