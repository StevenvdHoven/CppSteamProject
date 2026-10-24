#pragma once
#include "Core/Core.h"

// -----------------------
// DLL export / import
// -----------------------
// FlexSteam.dll is the runtime part of the Steam plugin: everything game code uses (packets,
// lobbies, the network thread). It ships with games, unlike the editor plugin FlexSteamEditor.dll.
// Including any of its headers links FlexSteam.lib (from Binaries/<Config>/, which game module
// projects already use as a library folder).
#ifdef FLEX_STEAM_EXPORTS
	#define FLEX_STEAM_API __declspec(dllexport)
#else
	#define FLEX_STEAM_API __declspec(dllimport)
	#pragma comment(lib, "FlexSteam.lib")
#endif
