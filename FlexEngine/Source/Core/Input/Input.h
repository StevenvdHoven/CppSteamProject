#pragma once
#include "Core/Core.h"
#include "Core/Math/Rect.h"
#include "Core/Math/Vector2.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>
#include <string_view>

union SDL_Event;

namespace Flex
{
	class InputMap;
	struct InputActionState;

	// Raw input: keyboard, mouse and gamepads, straight from SDL (main thread only). Quick to use in
	// game code while prototyping:
	//
	//     if (Input::WasKeyPressed(SDL_SCANCODE_SPACE)) { ... }
	//     const Vector2 move = Input::GetGamepadStick(0, false);
	//
	// "Pressed"/"Released" are true for the frame the change happened in (read them in Update systems:
	// a frame may run zero or several fixed steps). A press and release within one frame still counts.
	//
	// Player slots: gamepads get slots 0, 1, 2, ... in the order they connect, and a freed slot goes to
	// the next gamepad that connects. Player 0 also owns the keyboard and the mouse.
	//
	// Mapped input (actions from an InputMap asset, any device, per player): GetAction below.
	//
	// The frame loop feeds it: BeginFrame before polling SDL events, ProcessEvent for each event, then
	// Update (evaluates the input maps in use).
	// FlexRuntime feeds everything; the editor feeds keyboard and mouse only while playing with the
	// Viewport focused (so keys typed into editor fields never reach the game).
	class FLEX_API Input
	{
	public:
		static constexpr uint32_t MaxPlayers = 8;
		static constexpr float DefaultDeadZone = 0.15f;

		// ---- Feeding (the frame loop) -----------------------------------------------------------
		static void BeginFrame();                       // clears last frame's presses, releases, mouse delta and wheel
		static void ProcessEvent(const SDL_Event& event);
		static void Update();                           // after the events: every input map in use evaluates its actions
		static uint64_t GetFrame();                     // counts BeginFrame calls
		static bool IsKeyboardOrMouseEvent(const SDL_Event& event); // keys, mouse buttons and wheel (not motion)
		static void ReleaseKeyboardMouse();             // focus lost: everything held is released (Released edges)
		// Where the game is drawn, in window coordinates, and its size in pixels: mouse positions are
		// given in those pixels (FlexRuntime: the whole window; the editor: the Viewport's image).
		static void SetViewport(const Rect& windowRect, const Vector2& pixelSize);

		// ---- Keyboard ----------------------------------------------------------------------------
		static bool IsKeyDown(SDL_Scancode key);
		static bool WasKeyPressed(SDL_Scancode key);
		static bool WasKeyReleased(SDL_Scancode key);

		// ---- Mouse (player 0) ------------------------------------------------------------------------
		// button: SDL_BUTTON_LEFT, SDL_BUTTON_MIDDLE, SDL_BUTTON_RIGHT, SDL_BUTTON_X1, SDL_BUTTON_X2.
		static bool IsMouseButtonDown(uint8_t button);
		static bool WasMouseButtonPressed(uint8_t button);
		static bool WasMouseButtonReleased(uint8_t button);
		static Vector2 GetMousePosition();   // pixels inside the game viewport (0,0 = top-left)
		static Vector2 GetMouseDelta();      // pixels moved this frame
		static Vector2 GetMouseWheel();      // wheel steps this frame (y > 0 = away from the user)
		static bool IsMouseInViewport();

		// ---- Gamepads ------------------------------------------------------------------------------
		static bool IsGamepadConnected(uint32_t player);
		static std::string GetGamepadName(uint32_t player);
		static bool IsGamepadButtonDown(uint32_t player, SDL_GamepadButton button);
		static bool WasGamepadButtonPressed(uint32_t player, SDL_GamepadButton button);
		static bool WasGamepadButtonReleased(uint32_t player, SDL_GamepadButton button);
		// Sticks -1..1 (y down), triggers 0..1, with the dead zone applied per axis.
		static float GetGamepadAxis(uint32_t player, SDL_GamepadAxis axis);
		// A whole stick with a round dead zone (better for movement than two separate axes).
		static Vector2 GetGamepadStick(uint32_t player, bool bRightStick);
		static float GetGamepadAxisRaw(uint32_t player, SDL_GamepadAxis axis); // no dead zone
		static void SetDeadZone(float deadZone);
		static float GetDeadZone();
		// Strength 0-1 per motor. False when the gamepad can't rumble.
		static bool Rumble(uint32_t player, float lowFrequency, float highFrequency, uint32_t milliseconds);

		// Which gamepad a player has (0 = none), and moving a gamepad to another player (the two swap).
		static SDL_JoystickID GetPlayerGamepad(uint32_t player);
		static bool SetPlayerGamepad(uint32_t player, SDL_JoystickID gamepad);

		// ---- Mapped input ------------------------------------------------------------------------
		// An action of an input map for one player ("Move", "Jump"): its value and Pressed/Released this
		// frame. An empty state for an unknown action.
		static const InputActionState& GetAction(const InputMap& map, std::string_view action, uint32_t player = 0);
	};
}
