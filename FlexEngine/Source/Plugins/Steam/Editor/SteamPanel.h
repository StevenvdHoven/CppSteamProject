#pragma once
#include "Plugins/Steam/Runtime/SteamNetwork.h"
#include "Editor/EditorWindow.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace Flex
{
	class Project;
}

namespace Flex::SteamEditor
{
	// The project's [Steam version=1] section of the .flexproj.
	struct SteamProjectSettings
	{
		static constexpr uint32_t CurrentVersion = 1;

		bool bEnabled = false;
		uint32_t AppId = Steam::Session::DevelopmentAppId;

		static SteamProjectSettings Load(Project& project);
		bool Save(Project& project) const;
	};

	// Steam is used when the project turns it on, or when its game code includes the plugin's headers
	// (then builds need FlexSteam.dll even if the setting was forgotten).
	bool ProjectUsesSteam(Project& project);

	// Windows > Steam: the connection to Steam, the project's Steam settings, the lobby (create, invite,
	// kick, ban, leave), lobby events and network statistics.
	class SteamPanel final : public EditorWindow
	{
	public:
		SteamPanel();

		void Draw() override;

		// Remembers this frame's lobby events for the log (called every frame, also while closed).
		void CollectEvents();

	private:
		void DrawStatus();
		void DrawSettings();
		void DrawLobby();
		void DrawMembers();
		void DrawFriends();
		void DrawBans();
		void DrawEvents();
		void DrawNetwork();

	private:
		SteamProjectSettings Settings;
		bool bSettingsLoaded = false;
		bool bSettingsChanged = false; // saved, applies after restarting the editor
		int MaxMembers = static_cast<int>(Steam::Lobby::DefaultMaxMembers);
		int Visibility = 0;

		std::deque<std::string> EventLog; // newest last
		std::vector<Steam::Friend> Friends;
		std::chrono::steady_clock::time_point FriendsRefreshed;
		char FriendFilter[64]{};
	};
}
