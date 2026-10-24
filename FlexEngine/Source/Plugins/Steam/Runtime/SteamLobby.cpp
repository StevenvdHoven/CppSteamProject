#include "Plugins/Steam/Runtime/SteamLobby.h"
#include "Plugins/Steam/Runtime/PacketRegistery.h"
#include "Plugins/Steam/Runtime/SteamInternal.h"
#include "Plugins/Steam/Runtime/SteamSession.h"
#include "Core/Project.h"

#include "Plugins/Steam/Runtime/SteamSdk.h"
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <unordered_map>

namespace Flex::Steam
{
	namespace
	{
		// Lobby data keys.
		constexpr const char* GameKey = "FlexGame";         // the project name
		constexpr const char* ProtocolKey = "FlexProtocol"; // PacketRegistery::GetProtocolHash, hex
		constexpr const char* BansKey = "FlexBans";         // banned SteamIds, space-separated

		// A kick: a lobby chat message from the host.
		constexpr char KickMagic[4] = { 'F', 'L', 'X', 'K' };
		struct KickMessage
		{
			char Magic[4];
			uint64_t Target;
			uint8_t bBan;
		};

		class Callbacks;

		struct LobbyState
		{
			CSteamID Id;
			CSteamID Host;
			bool bIsBusy = false;
			bool bIsCreating = false;
			bool bCancelJoin = false; // Leave was called while joining
			LobbyVisibility Visibility = LobbyVisibility::InviteOnly;
			std::vector<uint64_t> MemberOrder;                    // join order (Steam's own order isn't)
			std::unordered_map<uint64_t, std::string> RemovedBy;  // host: users it kicked/banned
			std::vector<LobbyEvent> Events;
			std::unique_ptr<Callbacks> SteamCallbacks;
		};
		LobbyState State;

		SteamId ToSteamId(CSteamID id)
		{
			return SteamId{ id.ConvertToUint64() };
		}

		CSteamID LocalUser()
		{
			return SteamUser()->GetSteamID();
		}

		void AddEvent(LobbyEventType type, SteamId user = {}, std::string message = {})
		{
			LobbyEvent event;
			event.Type = type;
			event.Lobby = ToSteamId(State.Id);
			event.User = user;
			event.Message = std::move(message);
			SDL_Log("Steam lobby: %s%s%s%s", ToString(type), user.IsValid() ? " " : "", user.IsValid() ? Session::GetName(user).c_str() : "",
				event.Message.empty() ? "" : (" (" + event.Message + ")").c_str());
			State.Events.push_back(std::move(event));
		}

		std::string GetProtocolText()
		{
			char text[32];
			std::snprintf(text, sizeof(text), "%016llX", static_cast<unsigned long long>(PacketRegistery::GetProtocolHash()));
			return text;
		}

		std::string GetGameName()
		{
			const Project* project = Project::GetActive();
			return project ? project->GetName() : std::string{};
		}

		std::vector<uint64_t> ReadBans(CSteamID lobby)
		{
			std::vector<uint64_t> bans;
			std::istringstream stream{ SteamMatchmaking()->GetLobbyData(lobby, BansKey) };
			uint64_t id = 0;
			while (stream >> id) {
				bans.push_back(id);
			}
			return bans;
		}

		void WriteBans(const std::vector<uint64_t>& bans)
		{
			std::string text;
			for (const uint64_t id : bans) {
				text += (text.empty() ? "" : " ") + std::to_string(id);
			}
			SteamMatchmaking()->SetLobbyData(State.Id, BansKey, text.c_str());
		}

		bool IsBanned(CSteamID lobby, CSteamID user)
		{
			const std::vector<uint64_t> bans = ReadBans(lobby);
			return std::find(bans.begin(), bans.end(), user.ConvertToUint64()) != bans.end();
		}

		bool AmHost()
		{
			return State.Id.IsValid() && State.Host == LocalUser();
		}

		void SendKick(CSteamID user, bool bBan)
		{
			KickMessage message{};
			std::memcpy(message.Magic, KickMagic, sizeof(KickMagic));
			message.Target = user.ConvertToUint64();
			message.bBan = bBan ? 1 : 0;
			SteamMatchmaking()->SendLobbyChatMsg(State.Id, &message, sizeof(message));
		}

		// Out of the lobby without a Left event (kicked, host left, refused).
		void LeaveQuietly()
		{
			if (State.Id.IsValid()) {
				SteamMatchmaking()->LeaveLobby(State.Id);
				SteamFriends()->ClearRichPresence();
			}
			State.Id = CSteamID();
			State.Host = CSteamID();
			State.MemberOrder.clear();
			State.RemovedBy.clear();
		}

		class Callbacks
		{
		public:
			STEAM_CALLBACK(Callbacks, OnLobbyEnter, LobbyEnter_t);
			STEAM_CALLBACK(Callbacks, OnChatUpdate, LobbyChatUpdate_t);
			STEAM_CALLBACK(Callbacks, OnDataUpdate, LobbyDataUpdate_t);
			STEAM_CALLBACK(Callbacks, OnChatMessage, LobbyChatMsg_t);
			STEAM_CALLBACK(Callbacks, OnJoinRequested, GameLobbyJoinRequested_t);
			STEAM_CALLBACK(Callbacks, OnInvite, LobbyInvite_t);

			void OnLobbyCreated(LobbyCreated_t* result, bool bIoFailure);
			CCallResult<Callbacks, LobbyCreated_t> CreateResult;
		};

		void Callbacks::OnLobbyCreated(LobbyCreated_t* result, bool bIoFailure)
		{
			if (bIoFailure || result->m_eResult != k_EResultOK) {
				State.bIsBusy = false;
				State.bIsCreating = false;
				AddEvent(LobbyEventType::EnterFailed, {}, "Steam could not create the lobby (result " + std::to_string(bIoFailure ? 0 : result->m_eResult) + ")");
			}
			// Success: LobbyEnter_t follows (in any order), OnLobbyEnter finishes the job.
		}

		void Callbacks::OnLobbyEnter(LobbyEnter_t* entered)
		{
			const CSteamID lobby{ entered->m_ulSteamIDLobby };
			const bool bWasCreating = State.bIsCreating;
			State.bIsBusy = false;
			State.bIsCreating = false;

			if (State.bCancelJoin) {
				State.bCancelJoin = false;
				SteamMatchmaking()->LeaveLobby(lobby);
				return;
			}
			if (entered->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess) {
				const char* reason = "the lobby can't be joined";
				switch (entered->m_EChatRoomEnterResponse) {
				case k_EChatRoomEnterResponseDoesntExist: reason = "the lobby doesn't exist any more"; break;
				case k_EChatRoomEnterResponseNotAllowed: reason = "not allowed (not invited?)"; break;
				case k_EChatRoomEnterResponseFull: reason = "the lobby is full"; break;
				case k_EChatRoomEnterResponseBanned: reason = "you are banned from this lobby"; break;
				case k_EChatRoomEnterResponseLimited: reason = "limited Steam accounts can't join lobbies"; break;
				default: break;
				}
				AddEvent(LobbyEventType::EnterFailed, {}, reason);
				return;
			}

			ISteamMatchmaking* matchmaking = SteamMatchmaking();
			State.Id = lobby;
			State.Host = matchmaking->GetLobbyOwner(lobby);
			State.MemberOrder.clear();
			State.RemovedBy.clear();

			if (State.Host == LocalUser()) {
				matchmaking->SetLobbyData(lobby, GameKey, GetGameName().c_str());
				matchmaking->SetLobbyData(lobby, ProtocolKey, GetProtocolText().c_str());
				matchmaking->SetLobbyData(lobby, BansKey, "");
				if (State.Visibility == LobbyVisibility::FriendsOnly) {
					SteamFriends()->SetRichPresence("connect", ("+connect_lobby " + std::to_string(lobby.ConvertToUint64())).c_str());
				}
				State.MemberOrder.push_back(LocalUser().ConvertToUint64());
				if (bWasCreating) {
					AddEvent(LobbyEventType::Created);
				}
			} else {
				// A client checks the host's build and its ban list before anything else.
				const std::string game = matchmaking->GetLobbyData(lobby, GameKey);
				const std::string protocol = matchmaking->GetLobbyData(lobby, ProtocolKey);
				std::string refusal;
				if (!game.empty() && game != GetGameName()) {
					refusal = "the lobby is for another game (" + game + ")";
				} else if (protocol != GetProtocolText()) {
					refusal = "the host runs another version of the game (packet protocol " + protocol + ", yours " + GetProtocolText() + ")";
				} else if (IsBanned(lobby, LocalUser())) {
					refusal = "you are banned from this lobby";
				}
				if (!refusal.empty()) {
					AddEvent(LobbyEventType::EnterFailed, {}, refusal);
					LeaveQuietly();
					return;
				}
				// The members already there, in Steam's order; the host first.
				State.MemberOrder.push_back(State.Host.ConvertToUint64());
				const int count = matchmaking->GetNumLobbyMembers(lobby);
				for (int index = 0; index < count; ++index) {
					const uint64_t member = matchmaking->GetLobbyMemberByIndex(lobby, index).ConvertToUint64();
					if (member != State.Host.ConvertToUint64()) {
						State.MemberOrder.push_back(member);
					}
				}
			}
			AddEvent(LobbyEventType::Entered, ToSteamId(State.Host));
		}

		void Callbacks::OnChatUpdate(LobbyChatUpdate_t* update)
		{
			if (CSteamID{ update->m_ulSteamIDLobby } != State.Id) {
				return;
			}
			const CSteamID user{ update->m_ulSteamIDUserChanged };
			const uint32_t change = update->m_rgfChatMemberStateChange;
			if (change & k_EChatMemberStateChangeEntered) {
				const uint64_t id = user.ConvertToUint64();
				if (std::find(State.MemberOrder.begin(), State.MemberOrder.end(), id) == State.MemberOrder.end()) {
					State.MemberOrder.push_back(id);
				}
				AddEvent(LobbyEventType::MemberJoined, ToSteamId(user));
				// A banned user who got in anyway (an old invite) is sent away again.
				if (AmHost() && IsBanned(State.Id, user)) {
					State.RemovedBy[id] = "banned";
					SendKick(user, true);
				}
				return;
			}

			if (user == State.Host && !AmHost()) {
				AddEvent(LobbyEventType::HostLeft, ToSteamId(user));
				LeaveQuietly();
				return;
			}
			std::string how = (change & k_EChatMemberStateChangeDisconnected) ? "disconnected" : "left";
			if (const auto removed = State.RemovedBy.find(user.ConvertToUint64()); removed != State.RemovedBy.end()) {
				how = removed->second;
				State.RemovedBy.erase(removed);
			}
			std::erase(State.MemberOrder, user.ConvertToUint64());
			AddEvent(LobbyEventType::MemberLeft, ToSteamId(user), how);
		}

		void Callbacks::OnDataUpdate(LobbyDataUpdate_t* update)
		{
			const CSteamID lobby{ update->m_ulSteamIDLobby };
			if (lobby != State.Id || update->m_ulSteamIDMember != update->m_ulSteamIDLobby || AmHost()) {
				return;
			}
			// Steam hands the lobby to someone else when the host goes: the session is over.
			if (SteamMatchmaking()->GetLobbyOwner(lobby) != State.Host) {
				AddEvent(LobbyEventType::HostLeft, ToSteamId(State.Host));
				LeaveQuietly();
				return;
			}
			if (IsBanned(lobby, LocalUser())) {
				AddEvent(LobbyEventType::Kicked, {}, "banned");
				LeaveQuietly();
			}
		}

		void Callbacks::OnChatMessage(LobbyChatMsg_t* message)
		{
			if (CSteamID{ message->m_ulSteamIDLobby } != State.Id) {
				return;
			}
			CSteamID sender;
			KickMessage kick{};
			EChatEntryType type = k_EChatEntryTypeInvalid;
			const int size = SteamMatchmaking()->GetLobbyChatEntry(State.Id, static_cast<int>(message->m_iChatID), &sender, &kick, sizeof(kick), &type);
			if (size != static_cast<int>(sizeof(KickMessage)) || std::memcmp(kick.Magic, KickMagic, sizeof(KickMagic)) != 0) {
				return;
			}
			// Only the host may kick, and only this user is meant.
			if (sender != State.Host || kick.Target != LocalUser().ConvertToUint64() || AmHost()) {
				return;
			}
			AddEvent(LobbyEventType::Kicked, {}, kick.bBan ? "banned" : "kicked");
			LeaveQuietly();
		}

		void Callbacks::OnJoinRequested(GameLobbyJoinRequested_t* request)
		{
			SDL_Log("Steam lobby: joining %s's lobby from the Steam overlay.", Session::GetName(ToSteamId(request->m_steamIDFriend)).c_str());
			Lobby::Join(ToSteamId(request->m_steamIDLobby));
		}

		void Callbacks::OnInvite(LobbyInvite_t* invite)
		{
			if (CGameID{ invite->m_ulGameID }.AppID() != Session::GetAppId()) {
				return;
			}
			LobbyEvent event;
			event.Type = LobbyEventType::InviteReceived;
			event.Lobby = SteamId{ invite->m_ulSteamIDLobby };
			event.User = SteamId{ invite->m_ulSteamIDUser };
			SDL_Log("Steam lobby: InviteReceived from %s", Session::GetName(event.User).c_str());
			State.Events.push_back(std::move(event));
		}
	}

	namespace Detail
	{
		void LobbyStartup()
		{
			State.SteamCallbacks = std::make_unique<Callbacks>();
		}

		void LobbyShutdown()
		{
			Lobby::Leave();
			State.SteamCallbacks.reset();
		}

		void LobbyBeginUpdate()
		{
			State.Events.clear();
		}

		void JoinFromCommandLine()
		{
			int count = 0;
			LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
			if (!arguments) {
				return;
			}
			for (int index = 0; index + 1 < count; ++index) {
				if (std::wcscmp(arguments[index], L"+connect_lobby") == 0) {
					const uint64_t lobby = std::wcstoull(arguments[index + 1], nullptr, 10);
					if (lobby != 0) {
						SDL_Log("Steam lobby: joining %llu (started from an invite).", static_cast<unsigned long long>(lobby));
						Lobby::Join(SteamId{ lobby });
					}
					break;
				}
			}
			LocalFree(arguments);
		}
	}

	bool Lobby::Create(uint32_t maxMembers, LobbyVisibility visibility)
	{
		if (!Session::IsInitialized() || State.bIsBusy || State.Id.IsValid()) {
			return false;
		}
		const ELobbyType type = visibility == LobbyVisibility::FriendsOnly ? k_ELobbyTypeFriendsOnly : k_ELobbyTypePrivate;
		const SteamAPICall_t call = SteamMatchmaking()->CreateLobby(type, static_cast<int>(std::clamp(maxMembers, 1u, 250u)));
		State.SteamCallbacks->CreateResult.Set(call, State.SteamCallbacks.get(), &Callbacks::OnLobbyCreated);
		State.Visibility = visibility;
		State.bIsBusy = true;
		State.bIsCreating = true;
		State.bCancelJoin = false;
		return true;
	}

	bool Lobby::Join(SteamId lobby)
	{
		if (!Session::IsInitialized() || State.bIsBusy || !lobby.IsValid()) {
			return false;
		}
		if (State.Id.IsValid()) {
			if (State.Id.ConvertToUint64() == lobby.Value) {
				return true;
			}
			Leave();
		}
		SteamMatchmaking()->JoinLobby(CSteamID(lobby.Value));
		State.Visibility = LobbyVisibility::InviteOnly;
		State.bIsBusy = true;
		State.bCancelJoin = false;
		return true;
	}

	void Lobby::Leave()
	{
		if (State.bIsBusy) {
			State.bCancelJoin = true;
		}
		if (!State.Id.IsValid()) {
			return;
		}
		AddEvent(LobbyEventType::Left);
		LeaveQuietly();
	}

	bool Lobby::OpenInviteDialog()
	{
		if (!AmHost()) {
			return false;
		}
		SteamFriends()->ActivateGameOverlayInviteDialog(State.Id);
		return true;
	}

	bool Lobby::InviteFriend(SteamId user)
	{
		if (!AmHost() || !user.IsValid()) {
			return false;
		}
		return SteamMatchmaking()->InviteUserToLobby(State.Id, CSteamID(user.Value));
	}

	bool Lobby::Kick(SteamId user, bool bBan)
	{
		const CSteamID target{ user.Value };
		if (!AmHost() || !user.IsValid() || target == LocalUser()) {
			return false;
		}
		if (bBan) {
			std::vector<uint64_t> bans = ReadBans(State.Id);
			if (std::find(bans.begin(), bans.end(), user.Value) == bans.end()) {
				bans.push_back(user.Value);
				WriteBans(bans);
			}
		}
		if (std::find(State.MemberOrder.begin(), State.MemberOrder.end(), user.Value) != State.MemberOrder.end()) {
			State.RemovedBy[user.Value] = bBan ? "banned" : "kicked";
			SendKick(target, bBan);
		}
		return true;
	}

	bool Lobby::Unban(SteamId user)
	{
		if (!AmHost()) {
			return false;
		}
		std::vector<uint64_t> bans = ReadBans(State.Id);
		if (std::erase(bans, user.Value) == 0) {
			return false;
		}
		WriteBans(bans);
		return true;
	}

	std::vector<SteamId> Lobby::GetBans()
	{
		std::vector<SteamId> bans;
		if (State.Id.IsValid()) {
			for (const uint64_t id : ReadBans(State.Id)) {
				bans.push_back(SteamId{ id });
			}
		}
		return bans;
	}

	bool Lobby::IsInLobby()
	{
		return State.Id.IsValid();
	}

	bool Lobby::IsBusy()
	{
		return State.bIsBusy;
	}

	bool Lobby::IsHost()
	{
		return Session::IsInitialized() && AmHost();
	}

	SteamId Lobby::GetId()
	{
		return ToSteamId(State.Id);
	}

	SteamId Lobby::GetHost()
	{
		return ToSteamId(State.Host);
	}

	std::vector<LobbyMember> Lobby::GetMembers()
	{
		std::vector<LobbyMember> members;
		for (const uint64_t id : State.MemberOrder) {
			LobbyMember member;
			member.Id = SteamId{ id };
			member.Name = Session::GetName(member.Id);
			member.bIsHost = id == State.Host.ConvertToUint64();
			members.push_back(std::move(member));
		}
		return members;
	}

	uint32_t Lobby::GetMaxMembers()
	{
		return State.Id.IsValid() ? static_cast<uint32_t>(SteamMatchmaking()->GetLobbyMemberLimit(State.Id)) : 0;
	}

	const std::vector<LobbyEvent>& Lobby::GetEvents()
	{
		return State.Events;
	}

	const char* ToString(LobbyEventType type)
	{
		switch (type) {
		case LobbyEventType::Created: return "Created";
		case LobbyEventType::Entered: return "Entered";
		case LobbyEventType::EnterFailed: return "EnterFailed";
		case LobbyEventType::Left: return "Left";
		case LobbyEventType::MemberJoined: return "MemberJoined";
		case LobbyEventType::MemberLeft: return "MemberLeft";
		case LobbyEventType::Kicked: return "Kicked";
		case LobbyEventType::HostLeft: return "HostLeft";
		case LobbyEventType::InviteReceived: return "InviteReceived";
		}
		return "?";
	}
}
