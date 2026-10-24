#pragma once
// Internal to FlexSteam.dll: the Steamworks SDK, included through here only. Valve's headers trip our
// warning level (C4996 strncpy is an error with SDL checks; C4828 non-UTF-8 bytes in comments), and
// they stay unchanged so the SDK can be updated by copying it over.
#include <Windows.h>
#include <shellapi.h>

#pragma warning(push)
#pragma warning(disable: 4996 4828)
#include <steam/steam_api.h>
#pragma warning(pop)
