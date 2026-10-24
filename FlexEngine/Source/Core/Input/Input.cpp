#include "Core/Input/Input.h"
#include "Core/Input/InputMap.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Flex
{
	namespace
	{
		constexpr int MouseButtonCount = 8;

		// Down state plus this frame's edges, for any number of buttons.
		template<int Count>
		struct Buttons
		{
			bool Down[Count] = {};
			bool Pressed[Count] = {};
			bool Released[Count] = {};

			void Set(int index, bool bDown)
			{
				if (index < 0 || index >= Count || Down[index] == bDown) {
					return;
				}
				Down[index] = bDown;
				(bDown ? Pressed : Released)[index] = true;
			}

			void ClearEdges()
			{
				std::memset(Pressed, 0, sizeof(Pressed));
				std::memset(Released, 0, sizeof(Released));
			}

			void ReleaseAll()
			{
				for (int index = 0; index < Count; ++index) {
					Set(index, false);
				}
			}

			bool IsDown(int index) const { return index >= 0 && index < Count && Down[index]; }
			bool WasPressed(int index) const { return index >= 0 && index < Count && Pressed[index]; }
			bool WasReleased(int index) const { return index >= 0 && index < Count && Released[index]; }
		};

		struct GamepadSlot
		{
			SDL_Gamepad* Gamepad = nullptr;
			SDL_JoystickID Id = 0;
			Buttons<SDL_GAMEPAD_BUTTON_COUNT> ButtonStates;
			float Axes[SDL_GAMEPAD_AXIS_COUNT] = {}; // raw -1..1 (triggers 0..1)
		};

		struct InputData
		{
			Buttons<SDL_SCANCODE_COUNT> Keys;
			Buttons<MouseButtonCount> MouseButtons;
			Vector2 MouseWindowPosition;
			Vector2 MouseDelta; // window coordinates
			Vector2 MouseWheel;
			Rect Viewport;
			Vector2 ViewportPixels;
			GamepadSlot Slots[Input::MaxPlayers];
			float DeadZone = Input::DefaultDeadZone;
			uint64_t Frame = 1;
		};
		InputData Data;

		GamepadSlot* FindSlot(SDL_JoystickID id)
		{
			for (GamepadSlot& slot : Data.Slots) {
				if (slot.Gamepad && slot.Id == id) {
					return &slot;
				}
			}
			return nullptr;
		}

		const GamepadSlot* GetSlot(uint32_t player)
		{
			return player < Input::MaxPlayers && Data.Slots[player].Gamepad ? &Data.Slots[player] : nullptr;
		}

		void AddGamepad(SDL_JoystickID id)
		{
			if (FindSlot(id)) {
				return;
			}
			for (uint32_t player = 0; player < Input::MaxPlayers; ++player) {
				GamepadSlot& slot = Data.Slots[player];
				if (!slot.Gamepad) {
					slot = GamepadSlot{};
					slot.Gamepad = SDL_OpenGamepad(id);
					if (!slot.Gamepad) {
						SDL_Log("Input: could not open gamepad %u: %s", static_cast<unsigned>(id), SDL_GetError());
						return;
					}
					slot.Id = id;
					SDL_Log("Input: gamepad '%s' is player %u.", SDL_GetGamepadName(slot.Gamepad), player);
					return;
				}
			}
			SDL_Log("Input: more than %u gamepads; '%s' is ignored.", Input::MaxPlayers, SDL_GetGamepadNameForID(id));
		}

		void RemoveGamepad(SDL_JoystickID id)
		{
			for (uint32_t player = 0; player < Input::MaxPlayers; ++player) {
				GamepadSlot& slot = Data.Slots[player];
				if (slot.Gamepad && slot.Id == id) {
					slot.ButtonStates.ReleaseAll();
					std::fill(std::begin(slot.Axes), std::end(slot.Axes), 0.0f);
					SDL_CloseGamepad(slot.Gamepad);
					slot.Gamepad = nullptr;
					slot.Id = 0;
					SDL_Log("Input: player %u's gamepad disconnected.", player);
					return;
				}
			}
		}

		float ApplyDeadZone(float value)
		{
			const float magnitude = std::abs(value);
			if (magnitude <= Data.DeadZone) {
				return 0.0f;
			}
			return std::copysign(std::min((magnitude - Data.DeadZone) / (1.0f - Data.DeadZone), 1.0f), value);
		}

		// Window coordinates -> game viewport pixels.
		Vector2 ToViewport(const Vector2& window)
		{
			if (Data.Viewport.width <= 0.0f || Data.Viewport.height <= 0.0f) {
				return window;
			}
			return Vector2((window.x - Data.Viewport.x) * Data.ViewportPixels.x / Data.Viewport.width,
				(window.y - Data.Viewport.y) * Data.ViewportPixels.y / Data.Viewport.height);
		}
	}

	void Input::BeginFrame()
	{
		Data.Frame++;
		Data.Keys.ClearEdges();
		Data.MouseButtons.ClearEdges();
		for (GamepadSlot& slot : Data.Slots) {
			slot.ButtonStates.ClearEdges();
		}
		Data.MouseDelta = {};
		Data.MouseWheel = {};
	}

	void Input::ProcessEvent(const SDL_Event& event)
	{
		switch (event.type) {
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
			if (!event.key.repeat) {
				Data.Keys.Set(event.key.scancode, event.type == SDL_EVENT_KEY_DOWN);
			}
			break;
		case SDL_EVENT_MOUSE_MOTION:
			Data.MouseWindowPosition = Vector2(event.motion.x, event.motion.y);
			Data.MouseDelta += Vector2(event.motion.xrel, event.motion.yrel);
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			Data.MouseWindowPosition = Vector2(event.button.x, event.button.y);
			Data.MouseButtons.Set(event.button.button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
			break;
		case SDL_EVENT_MOUSE_WHEEL:
			Data.MouseWheel += event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? Vector2(-event.wheel.x, -event.wheel.y) : Vector2(event.wheel.x, event.wheel.y);
			break;
		case SDL_EVENT_GAMEPAD_ADDED:
			AddGamepad(event.gdevice.which);
			break;
		case SDL_EVENT_GAMEPAD_REMOVED:
			RemoveGamepad(event.gdevice.which);
			break;
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
			if (GamepadSlot* slot = FindSlot(event.gbutton.which)) {
				slot->ButtonStates.Set(event.gbutton.button, event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
			}
			break;
		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			if (GamepadSlot* slot = FindSlot(event.gaxis.which); slot && event.gaxis.axis < SDL_GAMEPAD_AXIS_COUNT) {
				slot->Axes[event.gaxis.axis] = std::clamp(static_cast<float>(event.gaxis.value) / 32767.0f, -1.0f, 1.0f);
			}
			break;
		default:
			break;
		}
	}

	void Input::Update()
	{
		InputMap::UpdateAll(Data.Frame);
	}

	uint64_t Input::GetFrame()
	{
		return Data.Frame;
	}

	bool Input::IsKeyboardOrMouseEvent(const SDL_Event& event)
	{
		switch (event.type) {
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_WHEEL:
			return true;
		default:
			return false;
		}
	}

	void Input::ReleaseKeyboardMouse()
	{
		Data.Keys.ReleaseAll();
		Data.MouseButtons.ReleaseAll();
	}

	void Input::SetViewport(const Rect& windowRect, const Vector2& pixelSize)
	{
		Data.Viewport = windowRect;
		Data.ViewportPixels = pixelSize;
	}

	bool Input::IsKeyDown(SDL_Scancode key) { return Data.Keys.IsDown(key); }
	bool Input::WasKeyPressed(SDL_Scancode key) { return Data.Keys.WasPressed(key); }
	bool Input::WasKeyReleased(SDL_Scancode key) { return Data.Keys.WasReleased(key); }

	bool Input::IsMouseButtonDown(uint8_t button) { return Data.MouseButtons.IsDown(button); }
	bool Input::WasMouseButtonPressed(uint8_t button) { return Data.MouseButtons.WasPressed(button); }
	bool Input::WasMouseButtonReleased(uint8_t button) { return Data.MouseButtons.WasReleased(button); }

	Vector2 Input::GetMousePosition()
	{
		return ToViewport(Data.MouseWindowPosition);
	}

	Vector2 Input::GetMouseDelta()
	{
		return ToViewport(Data.MouseDelta) - ToViewport({});
	}

	Vector2 Input::GetMouseWheel()
	{
		return Data.MouseWheel;
	}

	bool Input::IsMouseInViewport()
	{
		const Vector2 position = GetMousePosition();
		const Vector2 size = Data.Viewport.width > 0.0f ? Data.ViewportPixels : Vector2{};
		return size.x <= 0.0f || (position.x >= 0.0f && position.y >= 0.0f && position.x < size.x && position.y < size.y);
	}

	bool Input::IsGamepadConnected(uint32_t player)
	{
		return GetSlot(player) != nullptr;
	}

	std::string Input::GetGamepadName(uint32_t player)
	{
		const GamepadSlot* slot = GetSlot(player);
		const char* name = slot ? SDL_GetGamepadName(slot->Gamepad) : nullptr;
		return name ? name : "";
	}

	bool Input::IsGamepadButtonDown(uint32_t player, SDL_GamepadButton button)
	{
		const GamepadSlot* slot = GetSlot(player);
		return slot && slot->ButtonStates.IsDown(button);
	}

	bool Input::WasGamepadButtonPressed(uint32_t player, SDL_GamepadButton button)
	{
		const GamepadSlot* slot = GetSlot(player);
		return slot && slot->ButtonStates.WasPressed(button);
	}

	bool Input::WasGamepadButtonReleased(uint32_t player, SDL_GamepadButton button)
	{
		const GamepadSlot* slot = GetSlot(player);
		return slot && slot->ButtonStates.WasReleased(button);
	}

	float Input::GetGamepadAxis(uint32_t player, SDL_GamepadAxis axis)
	{
		const GamepadSlot* slot = GetSlot(player);
		if (!slot || axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT) {
			return 0.0f;
		}
		return ApplyDeadZone(slot->Axes[axis]);
	}

	Vector2 Input::GetGamepadStick(uint32_t player, bool bRightStick)
	{
		const GamepadSlot* slot = GetSlot(player);
		if (!slot) {
			return {};
		}
		const Vector2 raw = bRightStick ? Vector2(slot->Axes[SDL_GAMEPAD_AXIS_RIGHTX], slot->Axes[SDL_GAMEPAD_AXIS_RIGHTY])
			: Vector2(slot->Axes[SDL_GAMEPAD_AXIS_LEFTX], slot->Axes[SDL_GAMEPAD_AXIS_LEFTY]);
		const float length = raw.Length();
		if (length <= Data.DeadZone) {
			return {};
		}
		const float scaled = std::min((length - Data.DeadZone) / (1.0f - Data.DeadZone), 1.0f);
		return raw * (scaled / length);
	}

	float Input::GetGamepadAxisRaw(uint32_t player, SDL_GamepadAxis axis)
	{
		const GamepadSlot* slot = GetSlot(player);
		return slot && axis >= 0 && axis < SDL_GAMEPAD_AXIS_COUNT ? slot->Axes[axis] : 0.0f;
	}

	void Input::SetDeadZone(float deadZone)
	{
		Data.DeadZone = std::clamp(deadZone, 0.0f, 0.95f);
	}

	float Input::GetDeadZone()
	{
		return Data.DeadZone;
	}

	bool Input::Rumble(uint32_t player, float lowFrequency, float highFrequency, uint32_t milliseconds)
	{
		const GamepadSlot* slot = GetSlot(player);
		const auto strength = [](float value) { return static_cast<Uint16>(std::clamp(value, 0.0f, 1.0f) * 65535.0f); };
		return slot && SDL_RumbleGamepad(slot->Gamepad, strength(lowFrequency), strength(highFrequency), milliseconds);
	}

	SDL_JoystickID Input::GetPlayerGamepad(uint32_t player)
	{
		const GamepadSlot* slot = GetSlot(player);
		return slot ? slot->Id : 0;
	}

	bool Input::SetPlayerGamepad(uint32_t player, SDL_JoystickID gamepad)
	{
		if (player >= MaxPlayers) {
			return false;
		}
		GamepadSlot* current = FindSlot(gamepad);
		if (!current) {
			return false;
		}
		std::swap(*current, Data.Slots[player]);
		return true;
	}

	const InputActionState& Input::GetAction(const InputMap& map, std::string_view action, uint32_t player)
	{
		return map.GetState(action, player);
	}
}
