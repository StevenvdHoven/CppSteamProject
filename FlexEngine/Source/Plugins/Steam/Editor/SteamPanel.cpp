#include "Plugins/Steam/Editor/SteamPanel.h"
#include "Core/FileSystem.h"
#include "Core/Project.h"
#include "Core/Serialization/TextDocument.h"
#include "Core/Serialization/TextValue.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace Flex::SteamEditor
{
	using namespace Flex::Steam;

	namespace
	{
		constexpr const char* SettingsSection = "Steam";
		constexpr size_t MaxEventLines = 100;
		constexpr auto FriendsRefreshInterval = std::chrono::seconds(2);

		const ImVec4 ErrorColor{ 1.0f, 0.45f, 0.45f, 1.0f };
		const ImVec4 OkColor{ 0.45f, 0.85f, 0.45f, 1.0f };

		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
			return text;
		}

		std::string FormatBytes(uint64_t bytes)
		{
			char text[32];
			if (bytes >= 1024ull * 1024ull) {
				std::snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
			} else if (bytes >= 1024ull) {
				std::snprintf(text, sizeof(text), "%.1f KB", static_cast<double>(bytes) / 1024.0);
			} else {
				std::snprintf(text, sizeof(text), "%llu B", static_cast<unsigned long long>(bytes));
			}
			return text;
		}
	}

	SteamProjectSettings SteamProjectSettings::Load(Project& project)
	{
		SteamProjectSettings settings;
		if (const TextSection* section = project.GetDocument().FindSection(SettingsSection)) {
			if (const std::string* text = section->GetValue("Enabled")) {
				TextValue::ParseBool(*text, settings.bEnabled);
			}
			int64_t appId = 0;
			if (const std::string* text = section->GetValue("AppId"); text && TextValue::ParseInt(*text, appId) && appId > 0 && appId <= UINT32_MAX) {
				settings.AppId = static_cast<uint32_t>(appId);
			}
		}
		return settings;
	}

	bool SteamProjectSettings::Save(Project& project) const
	{
		TextSection& section = project.GetDocument().FindOrAddSection(SettingsSection);
		section.SetAttribute("version", std::to_string(CurrentVersion));
		section.SetValue("Enabled", TextValue::FormatBool(bEnabled));
		section.SetValue("AppId", TextValue::FormatInt(AppId), "Steam app id; 480 = Spacewar (Valve's test app) until the game has its own");
		return project.Save();
	}

	bool ProjectUsesSteam(Project& project)
	{
		if (SteamProjectSettings::Load(project).bEnabled) {
			return true;
		}
		std::error_code error;
		const std::filesystem::path source = project.GetRootDirectory() / "Source";
		if (!std::filesystem::is_directory(source, error)) {
			return false;
		}
		for (const auto& entry : std::filesystem::recursive_directory_iterator(source, error)) {
			const std::string extension = ToLower(PathToUtf8(entry.path().extension()));
			if (!entry.is_regular_file(error) || (extension != ".h" && extension != ".cpp")) {
				continue;
			}
			std::ifstream file{ entry.path() };
			std::stringstream text;
			text << file.rdbuf();
			if (text.str().find("Plugins/Steam/") != std::string::npos) {
				return true;
			}
		}
		return false;
	}

	SteamPanel::SteamPanel()
		: EditorWindow("Steam")
	{
	}

	void SteamPanel::CollectEvents()
	{
		for (const LobbyEvent& event : Lobby::GetEvents()) {
			std::string line = ToString(event.Type);
			if (event.User.IsValid()) {
				line += " " + Session::GetName(event.User);
			}
			if (!event.Message.empty()) {
				line += " (" + event.Message + ")";
			}
			EventLog.push_back(std::move(line));
			while (EventLog.size() > MaxEventLines) {
				EventLog.pop_front();
			}
		}
	}

	void SteamPanel::Draw()
	{
		if (!bSettingsLoaded) {
			if (Project* project = Project::GetActive()) {
				Settings = SteamProjectSettings::Load(*project);
			}
			bSettingsLoaded = true;
		}
		if (BeginWindow()) {
			DrawStatus();
			DrawSettings();
			if (Session::IsInitialized()) {
				ImGui::Separator();
				DrawLobby();
				DrawEvents();
				DrawNetwork();
			}
		}
		EndWindow();
	}

	void SteamPanel::DrawStatus()
	{
		if (Session::IsInitialized()) {
			const std::string name = Session::GetName(Session::GetLocalUser());
			ImGui::TextColored(OkColor, "Steam: signed in as %s (app %u)", name.c_str(), Session::GetAppId());
		} else if (!Session::GetInitError().empty()) {
			ImGui::TextColored(ErrorColor, "Steam not available: %s", Session::GetInitError().c_str());
			ImGui::TextDisabled("Start Steam and log in, then restart the editor.");
		} else {
			ImGui::TextDisabled("Steam is off for this project.");
		}
	}

	void SteamPanel::DrawSettings()
	{
		if (!ImGui::CollapsingHeader("Project settings", Session::IsInitialized() ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}
		Project* project = Project::GetActive();
		if (!project) {
			return;
		}
		bool bChanged = ImGui::Checkbox("Use Steam in this project", &Settings.bEnabled);
		int appId = static_cast<int>(Settings.AppId);
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::InputInt("App id", &appId, 0, 0) && appId > 0) {
			Settings.AppId = static_cast<uint32_t>(appId);
			bChanged = true;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(480 = Spacewar, for testing)");
		if (bChanged) {
			Settings.Save(*project);
			bSettingsChanged = true;
		}
		if (bSettingsChanged) {
			ImGui::TextDisabled("Saved. Applies after restarting the editor.");
		}
	}

	void SteamPanel::DrawLobby()
	{
		if (Lobby::IsBusy()) {
			ImGui::TextDisabled("Joining...");
			return;
		}
		if (!Lobby::IsInLobby()) {
			ImGui::TextUnformatted("Not in a lobby.");
			ImGui::SetNextItemWidth(100.0f);
			ImGui::InputInt("Max members", &MaxMembers);
			MaxMembers = std::clamp(MaxMembers, 1, 250);
			ImGui::SetNextItemWidth(160.0f);
			ImGui::Combo("Who can join", &Visibility, "Invited only\0Friends\0");
			if (ImGui::Button("Create lobby")) {
				Lobby::Create(static_cast<uint32_t>(MaxMembers), Visibility == 1 ? LobbyVisibility::FriendsOnly : LobbyVisibility::InviteOnly);
			}
			ImGui::SameLine();
			ImGui::TextDisabled("Friends join through your invite (Steam overlay) or their friends list.");
			return;
		}

		const std::vector<LobbyMember> members = Lobby::GetMembers();
		ImGui::Text("Lobby %llu: %zu / %u members, host %s%s", static_cast<unsigned long long>(Lobby::GetId().Value), members.size(),
			Lobby::GetMaxMembers(), Session::GetName(Lobby::GetHost()).c_str(), Lobby::IsHost() ? " (you)" : "");
		if (Lobby::IsHost()) {
			if (ImGui::Button("Invite friends...")) {
				Lobby::OpenInviteDialog();
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(Steam overlay; works while a game window has focus)");
		}
		if (ImGui::Button("Leave lobby")) {
			Lobby::Leave();
			return;
		}
		ImGui::SameLine();
		ImGui::TextColored(Network::IsConnected() ? OkColor : ErrorColor, Network::IsConnected() ? "Network connected" : "Connecting to the host...");

		DrawMembers();
		if (Lobby::IsHost()) {
			DrawFriends();
			DrawBans();
		}
	}

	void SteamPanel::DrawMembers()
	{
		const std::vector<ConnectionInfo> connections = Network::GetConnections();
		const SteamId self = Session::GetLocalUser();
		if (!ImGui::BeginTable("Members", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
			return;
		}
		ImGui::TableSetupColumn("Member");
		ImGui::TableSetupColumn("Role");
		ImGui::TableSetupColumn("Ping");
		ImGui::TableSetupColumn("Quality");
		ImGui::TableSetupColumn("");
		ImGui::TableHeadersRow();
		for (const LobbyMember& member : Lobby::GetMembers()) {
			ImGui::PushID(static_cast<int>(member.Id.Value ^ (member.Id.Value >> 32)));
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(member.Name.c_str());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(member.bIsHost ? "Host" : (member.Id == self ? "You" : "Client"));
			const auto connection = std::find_if(connections.begin(), connections.end(), [&](const ConnectionInfo& info) { return info.User == member.Id; });
			ImGui::TableNextColumn();
			if (member.Id == self) {
				ImGui::TextDisabled("-");
			} else if (connection == connections.end()) {
				ImGui::TextDisabled(Lobby::IsHost() ? "not connected" : "-");
			} else if (!connection->bIsConnected) {
				ImGui::TextDisabled("connecting");
			} else {
				ImGui::Text("%d ms", connection->PingMilliseconds);
			}
			ImGui::TableNextColumn();
			if (connection != connections.end() && connection->bIsConnected) {
				ImGui::Text("%.0f%%", connection->Quality * 100.0f);
			}
			ImGui::TableNextColumn();
			if (Lobby::IsHost() && member.Id != self) {
				if (ImGui::SmallButton("Kick")) {
					Lobby::Kick(member.Id, false);
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Ban")) {
					Lobby::Kick(member.Id, true);
				}
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	void SteamPanel::DrawFriends()
	{
		if (!ImGui::CollapsingHeader("Invite a friend")) {
			return;
		}
		const auto now = std::chrono::steady_clock::now();
		if (Friends.empty() || now - FriendsRefreshed > FriendsRefreshInterval) {
			Friends = Session::GetFriends();
			FriendsRefreshed = now;
		}
		ImGui::SetNextItemWidth(200.0f);
		ImGui::InputTextWithHint("##FriendFilter", "Search friends", FriendFilter, sizeof(FriendFilter));
		const std::string filter = ToLower(FriendFilter);
		if (ImGui::BeginChild("Friends", ImVec2(0.0f, 160.0f), ImGuiChildFlags_Borders)) {
			for (const Friend& entry : Friends) {
				if (!filter.empty() && ToLower(entry.Name).find(filter) == std::string::npos) {
					continue;
				}
				ImGui::PushID(static_cast<int>(entry.Id.Value ^ (entry.Id.Value >> 32)));
				if (ImGui::SmallButton("Invite")) {
					Lobby::InviteFriend(entry.Id);
				}
				ImGui::SameLine();
				if (entry.bIsPlayingThisGame) {
					ImGui::TextColored(OkColor, "%s (playing)", entry.Name.c_str());
				} else if (entry.bIsOnline) {
					ImGui::TextUnformatted(entry.Name.c_str());
				} else {
					ImGui::TextDisabled("%s (offline)", entry.Name.c_str());
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
	}

	void SteamPanel::DrawBans()
	{
		const std::vector<SteamId> bans = Lobby::GetBans();
		if (!ImGui::CollapsingHeader(("Banned (" + std::to_string(bans.size()) + ")###Banned").c_str())) {
			return;
		}
		if (bans.empty()) {
			ImGui::TextDisabled("Nobody.");
		}
		for (const SteamId user : bans) {
			ImGui::PushID(static_cast<int>(user.Value ^ (user.Value >> 32)));
			if (ImGui::SmallButton("Unban")) {
				Lobby::Unban(user);
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(Session::GetName(user).c_str());
			ImGui::PopID();
		}
	}

	void SteamPanel::DrawEvents()
	{
		if (!ImGui::CollapsingHeader("Lobby events")) {
			return;
		}
		if (ImGui::BeginChild("Events", ImVec2(0.0f, 120.0f), ImGuiChildFlags_Borders)) {
			for (const std::string& line : EventLog) {
				ImGui::TextUnformatted(line.c_str());
			}
			if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
				ImGui::SetScrollHereY(1.0f);
			}
		}
		ImGui::EndChild();
	}

	void SteamPanel::DrawNetwork()
	{
		if (!ImGui::CollapsingHeader("Network")) {
			return;
		}
		if (!Network::IsActive()) {
			ImGui::TextDisabled("Starts when you're in a lobby.");
			return;
		}
		const NetworkStats stats = Network::GetStats();
		ImGui::Text("Sent %llu (%s), received %llu (%s)", static_cast<unsigned long long>(stats.PacketsSent), FormatBytes(stats.BytesSent).c_str(),
			static_cast<unsigned long long>(stats.PacketsReceived), FormatBytes(stats.BytesReceived).c_str());
		ImGui::Text("Relayed %llu, handled locally %llu, dropped %llu", static_cast<unsigned long long>(stats.PacketsRelayed),
			static_cast<unsigned long long>(stats.PacketsHandledLocally), static_cast<unsigned long long>(stats.PacketsDropped));
		if (stats.Types.empty() || !ImGui::BeginTable("PacketTypes", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
			return;
		}
		ImGui::TableSetupColumn("Packet");
		ImGui::TableSetupColumn("Sent");
		ImGui::TableSetupColumn("Bytes sent");
		ImGui::TableSetupColumn("Received");
		ImGui::TableSetupColumn("Bytes received");
		ImGui::TableHeadersRow();
		for (const PacketTypeStats& type : stats.Types) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(type.Name.c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%llu", static_cast<unsigned long long>(type.Sent));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(FormatBytes(type.BytesSent).c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%llu", static_cast<unsigned long long>(type.Received));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(FormatBytes(type.BytesReceived).c_str());
		}
		ImGui::EndTable();
	}
}
