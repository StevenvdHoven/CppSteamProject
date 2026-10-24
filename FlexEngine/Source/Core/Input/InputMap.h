#pragma once
#include "Core/Assets/Asset.h"
#include "Core/Core.h"
#include "Core/Input/Input.h"
#include "Core/Math/Vector2.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Flex
{
	enum class InputActionType : uint8_t
	{
		Button, // on/off (jump, fire); Value.x is 1 while active
		Axis,   // one value -1..1 (throttle, turn); Value.x
		Vector2 // two values, length up to 1 (move, aim)
	};

	enum class InputDevice : uint8_t
	{
		Key,           // Code = SDL_Scancode (player 0)
		MouseButton,   // Code = SDL_BUTTON_LEFT ... SDL_BUTTON_X2 (player 0)
		MouseWheel,    // Code = 0 (x) or 1 (y): wheel steps this frame (player 0)
		GamepadButton, // Code = SDL_GamepadButton
		GamepadAxis    // Code = SDL_GamepadAxis: sticks -1..1, triggers 0..1
	};

	enum class InputComponent : uint8_t
	{
		X,
		Y
	};

	// One input that drives an action. Composites (WASD, arrows, D-pad) are several bindings with a
	// Component and a Scale: "A" = X with Scale -1, "D" = X with Scale 1, "W" = Y with Scale -1 (y down).
	struct InputBinding
	{
		InputDevice Device = InputDevice::Key;
		int32_t Code = -1;                               // -1 = not set / not recognised
		InputComponent Component = InputComponent::X;    // Vector2 actions: which component it adds to
		float Scale = 1.0f;                              // the value is multiplied by it (-1 flips)
		float DeadZone = -1.0f;                          // GamepadAxis: < 0 uses Input::GetDeadZone()
		std::string UnknownCode;                         // a code name from the file that wasn't recognised (kept)
	};

	struct InputAction
	{
		std::string Name;
		InputActionType Type = InputActionType::Button;
		std::vector<InputBinding> Bindings;
	};

	// An action's value this frame, for one player.
	struct InputActionState
	{
		Vector2 Value;          // Button: x = 1 while active; Axis: x; Vector2: both
		bool bIsActive = false; // Button: held; Axis/Vector2: not zero
		bool bPressed = false;  // became active this frame (a tap within one frame counts)
		bool bReleased = false; // stopped being active this frame

		float GetAxis() const { return Value.x; }
	};

	// The actions of an input map, as stored in a .inputmap file (FlexEngine text format):
	//
	//     [InputMap version=1]
	//     [Action version=1]
	//     Name = "Move"
	//     Type = "Vector2"
	//     [Binding version=1]
	//     Device = "Key"
	//     Code = "W"
	//     Component = "Y"
	//     Scale = -1
	//     DeadZone = -1
	//
	// Every [Binding] belongs to the [Action] before it. Codes are stored by name (SDL's key names,
	// SDL's gamepad button/axis names, Left/Middle/Right/X1/X2, X/Y), never as numbers.
	struct FLEX_API InputMapDefinition
	{
		static constexpr uint32_t Version = 1;
		static constexpr uint32_t ActionVersion = 1;
		static constexpr uint32_t BindingVersion = 1;

		std::vector<InputAction> Actions;

		int FindAction(std::string_view name) const; // -1 when missing

		// Problems that don't stop loading (unknown codes, types) go to outWarnings.
		bool Load(const std::filesystem::path& file, std::vector<std::string>& outWarnings, std::string& outError);
		bool Save(const std::filesystem::path& file, std::string& outError) const;
	};

	FLEX_API const char* ToString(InputActionType type);
	FLEX_API const char* ToString(InputDevice device);
	FLEX_API const char* ToString(InputComponent component);
	FLEX_API bool ParseInputActionType(std::string_view text, InputActionType& outType);
	FLEX_API bool ParseInputDevice(std::string_view text, InputDevice& outDevice);
	FLEX_API bool ParseInputComponent(std::string_view text, InputComponent& outComponent);
	// "Space", "a", "leftx", "Left", "Y", ... ("" for codes the device doesn't have).
	FLEX_API std::string GetInputCodeName(InputDevice device, int32_t code);
	FLEX_API int32_t ParseInputCode(InputDevice device, std::string_view name); // -1 when unknown

	// An InputMap asset (.inputmap): the actions and, while the game runs, their state per player.
	// States are evaluated once per frame (Input::Update) for every player the map was asked about,
	// so Pressed/Released are exact even when a system only looks now and then.
	class FLEX_API InputMap final : public Asset
	{
	public:
		static constexpr const char* TypeName = "InputMap";
		static constexpr const char* FileExtension = ".inputmap";

		InputMap();
		explicit InputMap(InputMapDefinition definition);
		~InputMap() override;

		static std::shared_ptr<InputMap> LoadFromFile(const std::filesystem::path& file, std::string& outError);

		const char* GetTypeName() const override { return TypeName; }
		size_t GetMemorySize() const override;

		const InputMapDefinition& GetDefinition() const { return Definition; }
		// Replaces the actions in place (the editor after saving): every user sees them at once.
		void SetDefinition(InputMapDefinition definition);

		int FindAction(std::string_view name) const { return Definition.FindAction(name); }
		// An empty state for an unknown action or player.
		const InputActionState& GetState(std::string_view action, uint32_t player = 0) const;
		const InputActionState& GetState(int actionIndex, uint32_t player = 0) const;

		// Input::Update calls it for every live map; not for game code.
		static void UpdateAll(uint64_t frame);

	private:
		void Evaluate(uint32_t player, uint64_t frame, bool bFirstTime) const;

		struct PlayerStates
		{
			bool bInUse = false;
			uint64_t Frame = 0;
			std::vector<InputActionState> Actions;
		};

		InputMapDefinition Definition;
		mutable std::array<PlayerStates, Input::MaxPlayers> Players;
	};
}
