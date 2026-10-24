#include "NetChat.h"
#include "Core/Game/SystemRegistery.h"
#include "Core/Input/Input.h"
#include "Core/Math/MathUtils.h"
#include "Core/World/GameObject.h"
#include "Core/World/Transform.h"
#include "Core/World/World.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <mutex>
#include <vector>

using namespace Flex;
using namespace Flex::Steam;

namespace NetChat
{
	FLEX_REGISTER_COMPONENT(ShipControl)
	FLEX_REGISTER_STEAM_PACKET(ChatMessage)
	FLEX_REGISTER_STEAM_PACKET(SteerInput)
	FLEX_REGISTER_STEAM_PACKET(ShipState)

	namespace
	{
		// Chat lines arrive on the network thread and are printed on the main thread (where Steam's
		// friend names may be looked up).
		std::mutex ChatMutex;
		std::vector<std::pair<SteamId, std::string>> ChatLines;

		const char* const CannedLines[] = { "Hello!", "Follow me!", "Good game!" };

		// Arrow keys, the left stick or the D-pad as -1/0/1 per axis (y down).
		void ReadSteering(int8_t& x, int8_t& y)
		{
			const Vector2 stick = Input::GetGamepadStick(0, false);
			const auto direction = [](bool bNegative, bool bPositive, float axis) {
				return static_cast<int8_t>(((bPositive || axis > 0.5f) ? 1 : 0) - ((bNegative || axis < -0.5f) ? 1 : 0));
			};
			x = direction(Input::IsKeyDown(SDL_SCANCODE_LEFT) || Input::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_DPAD_LEFT),
				Input::IsKeyDown(SDL_SCANCODE_RIGHT) || Input::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_DPAD_RIGHT), stick.x);
			y = direction(Input::IsKeyDown(SDL_SCANCODE_UP) || Input::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_DPAD_UP),
				Input::IsKeyDown(SDL_SCANCODE_DOWN) || Input::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_DPAD_DOWN), stick.y);
		}

		// The host moves the ships (or anyone, when there's no lobby).
		bool IsInCharge()
		{
			return !Lobby::IsInLobby() || Lobby::IsHost();
		}
	}

	// ---- Packets --------------------------------------------------------------------------------

	HandleResult ChatMessage::OnReceive(const PacketContext& context)
	{
		std::scoped_lock lock{ ChatMutex };
		ChatLines.emplace_back(context.Sender, std::move(Text));
		return HandleResult::Done;
	}

	void SteerInput::OnReceiveMain(World& world, const PacketContext&)
	{
		world.Each<ShipControl>([this](ShipControl& ship) {
			ship.Input += Vector2(static_cast<float>(X), static_cast<float>(Y));
		});
	}

	void ShipState::OnReceiveMain(World& world, const PacketContext&)
	{
		world.Each<GameObjectInfo, Transform>([this](const GameObjectInfo& info, Transform& transform) {
			if (info.Id == Ship) {
				transform.Position = Position;
				transform.Rotation = Rotation;
			}
		});
	}

	// ---- Systems --------------------------------------------------------------------------------

	void PrintControls(World&, float)
	{
		SDL_Log("NetChat: F1 create lobby, F2 invite friends, F3 leave, 1/2/3 chat, arrow keys steer.");
	}
	FLEX_REGISTER_SYSTEM(Start, PrintControls)

	// Update: the keys for the lobby and the chat.
	void HandleKeys(World&, float)
	{
		if (Input::WasKeyPressed(SDL_SCANCODE_F1) && !Lobby::IsInLobby()) {
			Lobby::Create(4);
		}
		if (Input::WasKeyPressed(SDL_SCANCODE_F2) && !Lobby::OpenInviteDialog()) {
			SDL_Log("NetChat: only the host can invite (press F1 first).");
		}
		if (Input::WasKeyPressed(SDL_SCANCODE_F3)) {
			Lobby::Leave();
		}
		const SDL_Scancode chatKeys[] = { SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3 };
		for (int index = 0; index < 3; ++index) {
			if (Input::WasKeyPressed(chatKeys[index]) && !Network::Send(ChatMessage{ CannedLines[index] }, Target::ToEveryone())) {
				SDL_Log("NetChat: not in a lobby, nobody hears you.");
			}
		}
	}
	FLEX_REGISTER_SYSTEM(Update, HandleKeys)

	// Update: what happened in the lobby, and the chat lines that came in.
	void ShowLobbyAndChat(World&, float)
	{
		for (const LobbyEvent& event : Lobby::GetEvents()) {
			switch (event.Type) {
			case LobbyEventType::Created:
				SDL_Log("NetChat: lobby created; press F2 to invite friends.");
				break;
			case LobbyEventType::MemberJoined:
				SDL_Log("NetChat: %s joined.", Session::GetName(event.User).c_str());
				if (Lobby::IsHost()) {
					Network::Send(ChatMessage{ "Welcome, " + Session::GetName(event.User) + "!" }, Target::ToEveryone());
				}
				break;
			case LobbyEventType::MemberLeft:
				SDL_Log("NetChat: %s %s.", Session::GetName(event.User).c_str(), event.Message.c_str());
				break;
			case LobbyEventType::EnterFailed:
				SDL_Log("NetChat: could not join: %s.", event.Message.c_str());
				break;
			case LobbyEventType::Kicked:
				SDL_Log("NetChat: the host removed you (%s).", event.Message.c_str());
				break;
			case LobbyEventType::HostLeft:
				SDL_Log("NetChat: the host left, the session is over.");
				break;
			default:
				break;
			}
		}

		std::vector<std::pair<SteamId, std::string>> lines;
		{
			std::scoped_lock lock{ ChatMutex };
			std::swap(lines, ChatLines);
		}
		for (const auto& [sender, text] : lines) {
			SDL_Log("[%s] %s", Session::GetName(sender).c_str(), text.c_str());
		}
	}
	FLEX_REGISTER_SYSTEM(Update, ShowLobbyAndChat)

	// FixedUpdate: everyone sends their steering to the host (the host sends it to itself: the same path).
	void SendSteering(World& world, float)
	{
		int8_t x = 0;
		int8_t y = 0;
		ReadSteering(x, y);
		if (x == 0 && y == 0) {
			return;
		}
		if (Network::IsActive()) {
			Network::Send(SteerInput{ x, y }, Target::ToHost());
		} else {
			world.Each<ShipControl>([x, y](ShipControl& ship) { ship.Input += Vector2(x, y); });
		}
	}
	FLEX_REGISTER_SYSTEM_ORDER(FixedUpdate, SendSteering, 0)

	// FixedUpdate, after SendSteering: the host moves the ship by everyone's steering and tells the others.
	void MoveShips(World& world, float deltaTime)
	{
		if (!IsInCharge()) {
			return;
		}
		world.Each<GameObjectInfo, Transform, ShipControl>([deltaTime](const GameObjectInfo& info, Transform& transform, ShipControl& ship) {
			if (ship.Input.LengthSquared() > 0.0f) {
				const Vector2 direction = ship.Input.Normalized();
				transform.Position += direction * ship.Speed * deltaTime;
				transform.Position.x = std::clamp(transform.Position.x, -300.0f, 300.0f);
				transform.Position.y = std::clamp(transform.Position.y, -160.0f, 160.0f);
				transform.Rotation = direction.Angle();
			}
			ship.Input = {};
			if (Network::IsActive()) {
				Network::Send(ShipState{ info.Id, transform.Position, transform.Rotation }, Target::ToOthers());
			}
		});
	}
	FLEX_REGISTER_SYSTEM_ORDER(FixedUpdate, MoveShips, 10)
}
