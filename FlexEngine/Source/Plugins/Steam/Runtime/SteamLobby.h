#pragma once
#include "Plugins/Steam/Runtime/SteamApi.h"
#include "Plugins/Steam/Runtime/SteamTypes.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Flex::Steam
{
	enum class LobbyVisibility : uint8_t
	{
		InviteOnly, // only people the host invites can join. The default.
		FriendsOnly // the host's friends can also join from their friends list ("Join Game")
	};

	enum class LobbyEventType : uint8_t
	{
		Created,        // you created a lobby and are its host (Entered follows)
		Entered,        // you are in a lobby (host or client)
		EnterFailed,    // joining failed (Message says why: full, gone, banned, another game version, ...)
		Left,           // you left (Leave, or Session::Shutdown)
		MemberJoined,   // User joined your lobby
		MemberLeft,     // User left your lobby (Message: "left", "disconnected", "kicked", "banned")
		Kicked,         // the host removed you (Message: "kicked" or "banned"); you're out of the lobby
		HostLeft,       // the host left: the session is over and you're out of the lobby
		InviteReceived  // User invited you to Lobby (accepting it in the Steam overlay joins by itself; or call Join)
	};

	struct LobbyEvent
	{
		LobbyEventType Type = LobbyEventType::Entered;
		SteamId Lobby;
		SteamId User;
		std::string Message;
	};

	struct LobbyMember
	{
		SteamId Id;
		std::string Name;
		bool bIsHost = false;
	};

	// One Steam lobby at a time (main thread only). The member who created it is the host and the
	// source of truth: only the host invites, kicks and bans. When the host leaves, the session ends
	// for everyone (HostLeft).
	//
	// Joining also works from outside the game: accepting an invite in the Steam overlay joins at once,
	// and a game started from an invite (+connect_lobby <id>) joins after Session::Init.
	//
	// Every lobby stores the game's name and packet protocol hash (PacketRegistery::GetProtocolHash);
	// a client with another build is refused (EnterFailed) instead of receiving packets it can't read.
	//
	// Kicks travel as a lobby chat message from the host (only the host's are obeyed) and bans are kept
	// in the lobby's data, so a banned user who joins again is sent away. The network connection is
	// closed and refused as well once the network thread exists (next patch).
	class FLEX_STEAM_API Lobby
	{
	public:
		static constexpr uint32_t DefaultMaxMembers = 4;

		// Creates a lobby with you as host (Created + Entered events). False when Steam isn't running
		// or you're already in a lobby or busy joining one.
		static bool Create(uint32_t maxMembers = DefaultMaxMembers, LobbyVisibility visibility = LobbyVisibility::InviteOnly);
		// Joins a lobby (Entered or EnterFailed). Leaves the current one first.
		static bool Join(SteamId lobby);
		static void Leave();

		// Host only: the Steam overlay's invite dialog, or one friend directly.
		static bool OpenInviteDialog();
		static bool InviteFriend(SteamId user);

		// Host only. A banned user can't join again until unbanned.
		static bool Kick(SteamId user, bool bBan = false);
		static bool Unban(SteamId user);
		static std::vector<SteamId> GetBans();

		static bool IsInLobby();
		static bool IsBusy(); // creating or joining
		static bool IsHost();
		static SteamId GetId();
		static SteamId GetHost();
		static std::vector<LobbyMember> GetMembers(); // the host first, then in join order
		static uint32_t GetMaxMembers();

		// What happened during the last Session::Update (cleared at the start of the next one).
		static const std::vector<LobbyEvent>& GetEvents();
	};

	FLEX_STEAM_API const char* ToString(LobbyEventType type);
}
