Steamworks SDK (Valve), vendored for the FlexSteam plugin.

include/steam/   the SDK's public/steam headers (include as <steam/steam_api.h>)
lib/x64/         steam_api64.lib (import library) and steam_api64.dll (redistributable)

Copied from SpaceAdventure/Thirdparty/steam (SDK files dated 2025-11-18;
interfaces SteamNetworkingSockets012, SteamNetworkingMessages002, SteamMatchMaking009).
Only the x64 files are kept. To update: download the latest SDK from
https://partner.steamgames.com/downloads/list and replace sdk/public/steam/*.h and
sdk/redistributable_bin/win64/steam_api64.{dll,lib}.

The Steamworks SDK is covered by the Steamworks SDK Access Agreement;
steam_api64.dll may be redistributed with games.
