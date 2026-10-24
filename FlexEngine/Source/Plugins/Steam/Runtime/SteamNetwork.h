#pragma once
// The one include for game code that uses the Steam plugin (FlexSteam.dll):
//
//     #include "Plugins/Steam/Runtime/SteamNetwork.h"
//
// Packets: FLEX_STEAM_PACKET / FLEX_REGISTER_STEAM_PACKET (Packet.h). Lobbies and sending come later.
#include "Plugins/Steam/Runtime/Packet.h"
#include "Plugins/Steam/Runtime/PacketRegistery.h"
#include "Plugins/Steam/Runtime/PacketStream.h"
#include "Plugins/Steam/Runtime/SteamApi.h"
#include "Plugins/Steam/Runtime/SteamTypes.h"
