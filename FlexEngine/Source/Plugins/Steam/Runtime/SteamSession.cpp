#include "Plugins/Steam/Runtime/SteamSession.h"
#include "Plugins/Steam/Runtime/Network.h"
#include "Plugins/Steam/Runtime/SteamInternal.h"
#include "Core/Game/SystemRegistery.h"
#include "Core/Project.h"
#include "Core/Serialization/TextDocument.h"
#include "Core/Serialization/TextValue.h"

#include "Plugins/Steam/Runtime/SteamSdk.h"
#include <SDL3/SDL.h>

#include <algorithm>
#include <string>

namespace Flex::Steam
{
	namespace
	{
		bool bInitialized = false;
		bool bInitAttempted = false;
		std::string InitError;
		uint32_t AppId = 0;

		// FlexSteam.dll is loaded by the game module that uses it, and the game module is unloaded by hot
		// reload. Steam, its callbacks and the SteamUpdate system live here, so the DLL stays loaded until
		// the process exits.
		void PinThisModule()
		{
			HMODULE module = nullptr;
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
				reinterpret_cast<LPCWSTR>(&PinThisModule), &module);
		}
		const bool bPinned = (PinThisModule(), true);

		uint32_t ReadProjectAppId()
		{
			if (Project* project = Project::GetActive()) {
				if (const TextSection* section = project->GetDocument().FindSection("Steam")) {
					int64_t value = 0;
					if (const std::string* text = section->GetValue("AppId"); text && TextValue::ParseInt(*text, value) && value > 0 && value <= UINT32_MAX) {
						return static_cast<uint32_t>(value);
					}
				}
			}
			return Session::DevelopmentAppId;
		}

		// Runs first in every frame of a game (the editor's Play mode included): Steam's callbacks, then the
		// packets that arrived for the main thread.
		void SteamUpdate(World& world, float)
		{
			Session::Update();
			Network::DispatchMainThread(&world);
		}
	}
	FLEX_REGISTER_SYSTEM_ORDER(Update, SteamUpdate, -1000)

	bool Session::Init(uint32_t appId, std::string* outError)
	{
		if (!bInitAttempted) {
			bInitAttempted = true;
			const std::wstring id = std::to_wstring(appId);
			SetEnvironmentVariableW(L"SteamAppId", id.c_str());
			SetEnvironmentVariableW(L"SteamGameId", id.c_str());

			SteamErrMsg message{};
			if (SteamAPI_InitEx(&message) == k_ESteamAPIInitResult_OK) {
				bInitialized = true;
				AppId = SteamUtils()->GetAppID();
				SDL_Log("Steam: signed in as %s (app %u).", SteamFriends()->GetPersonaName(), AppId);
				Detail::LobbyStartup();
				Detail::NetworkStartup();
				Detail::JoinFromCommandLine();
			} else {
				InitError = message[0] ? message : "Steam could not be started.";
				SDL_Log("Steam: not available: %s", InitError.c_str());
			}
		}
		if (!bInitialized && outError) {
			*outError = InitError;
		}
		return bInitialized;
	}

	bool Session::InitFromProject(std::string* outError)
	{
		return Init(ReadProjectAppId(), outError);
	}

	void Session::Shutdown()
	{
		if (!bInitialized) {
			return;
		}
		Detail::LobbyShutdown();
		Detail::NetworkShutdown();
		SteamAPI_Shutdown();
		bInitialized = false;
		SDL_Log("Steam: shut down.");
	}

	bool Session::IsInitialized()
	{
		return bInitialized;
	}

	const std::string& Session::GetInitError()
	{
		return InitError;
	}

	uint32_t Session::GetAppId()
	{
		return AppId;
	}

	void Session::Update()
	{
		if (!bInitAttempted) {
			InitFromProject();
		}
		Detail::LobbyBeginUpdate();
		if (bInitialized) {
			SteamAPI_RunCallbacks();
			Detail::NetworkUpdate();
		}
	}

	bool Session::RestartThroughSteamIfNeeded(uint32_t appId)
	{
		return SteamAPI_RestartAppIfNecessary(appId);
	}

	SteamId Session::GetLocalUser()
	{
		return bInitialized ? SteamId{ SteamUser()->GetSteamID().ConvertToUint64() } : SteamId{};
	}

	std::string Session::GetName(SteamId user)
	{
		if (!bInitialized || !user.IsValid()) {
			return {};
		}
		if (user == GetLocalUser()) {
			return SteamFriends()->GetPersonaName();
		}
		return SteamFriends()->GetFriendPersonaName(CSteamID(user.Value));
	}

	std::vector<Friend> Session::GetFriends()
	{
		std::vector<Friend> friends;
		if (!bInitialized) {
			return friends;
		}
		ISteamFriends* steamFriends = SteamFriends();
		const int count = steamFriends->GetFriendCount(k_EFriendFlagImmediate);
		friends.reserve(count > 0 ? count : 0);
		for (int index = 0; index < count; ++index) {
			const CSteamID id = steamFriends->GetFriendByIndex(index, k_EFriendFlagImmediate);
			Friend entry;
			entry.Id = SteamId{ id.ConvertToUint64() };
			entry.Name = steamFriends->GetFriendPersonaName(id);
			entry.bIsOnline = steamFriends->GetFriendPersonaState(id) != k_EPersonaStateOffline;
			FriendGameInfo_t game{};
			entry.bIsPlayingThisGame = steamFriends->GetFriendGamePlayed(id, &game) && game.m_gameID.AppID() == AppId;
			friends.push_back(std::move(entry));
		}
		std::sort(friends.begin(), friends.end(), [](const Friend& a, const Friend& b) {
			if (a.bIsOnline != b.bIsOnline) {
				return a.bIsOnline;
			}
			return a.Name < b.Name;
		});
		return friends;
	}
}
