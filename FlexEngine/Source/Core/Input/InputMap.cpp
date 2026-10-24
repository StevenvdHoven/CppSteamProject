#include "Core/Input/InputMap.h"
#include "Core/Assets/AssetManager.h"
#include "Core/FileSystem.h"
#include "Core/Serialization/TextArchive.h"
#include "Core/Serialization/TextDocument.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Flex
{
	namespace
	{
		constexpr const char* MapSection = "InputMap";
		constexpr const char* ActionSection = "Action";
		constexpr const char* BindingSection = "Binding";
		constexpr float ButtonThreshold = 0.5f; // an axis counts as a pressed button past this

		// Every live InputMap, so Input::Update can evaluate them (main thread only).
		std::vector<const InputMap*>& LiveMaps()
		{
			static std::vector<const InputMap*> maps;
			return maps;
		}

		const InputActionState EmptyState;

		const char* const MouseButtonNames[] = { "", "Left", "Middle", "Right", "X1", "X2" };

		std::shared_ptr<Asset> LoadInputMapAsset(const std::filesystem::path& file, std::string& outError)
		{
			return InputMap::LoadFromFile(file, outError);
		}
		const bool bLoaderRegistered = (AssetManager::RegisterFileLoader(InputMap::TypeName, &LoadInputMapAsset), true);

		// One binding's value this frame (before Scale), and whether its button went down this frame.
		struct BindingValue
		{
			float Value = 0.0f;
			bool bPressedThisFrame = false;
		};

		BindingValue Read(const InputBinding& binding, uint32_t player)
		{
			BindingValue result;
			const bool bKeyboardMouse = player == 0;
			switch (binding.Device) {
			case InputDevice::Key:
				if (bKeyboardMouse && binding.Code >= 0) {
					const SDL_Scancode key = static_cast<SDL_Scancode>(binding.Code);
					result.Value = Input::IsKeyDown(key) ? 1.0f : 0.0f;
					result.bPressedThisFrame = Input::WasKeyPressed(key);
				}
				break;
			case InputDevice::MouseButton:
				if (bKeyboardMouse && binding.Code >= 0) {
					const uint8_t button = static_cast<uint8_t>(binding.Code);
					result.Value = Input::IsMouseButtonDown(button) ? 1.0f : 0.0f;
					result.bPressedThisFrame = Input::WasMouseButtonPressed(button);
				}
				break;
			case InputDevice::MouseWheel:
				if (bKeyboardMouse) {
					const Vector2 wheel = Input::GetMouseWheel();
					result.Value = binding.Code == 1 ? wheel.y : (binding.Code == 0 ? wheel.x : 0.0f);
				}
				break;
			case InputDevice::GamepadButton:
				if (binding.Code >= 0) {
					const SDL_GamepadButton button = static_cast<SDL_GamepadButton>(binding.Code);
					result.Value = Input::IsGamepadButtonDown(player, button) ? 1.0f : 0.0f;
					result.bPressedThisFrame = Input::WasGamepadButtonPressed(player, button);
				}
				break;
			case InputDevice::GamepadAxis:
				if (binding.Code >= 0) {
					const float raw = Input::GetGamepadAxisRaw(player, static_cast<SDL_GamepadAxis>(binding.Code));
					const float deadZone = binding.DeadZone >= 0.0f ? std::min(binding.DeadZone, 0.95f) : Input::GetDeadZone();
					const float magnitude = std::abs(raw);
					result.Value = magnitude <= deadZone ? 0.0f : std::copysign(std::min((magnitude - deadZone) / (1.0f - deadZone), 1.0f), raw);
				}
				break;
			}
			return result;
		}

		void SerializeBinding(Archive& archive, InputBinding& binding, std::vector<std::string>* warnings)
		{
			archive.SerializeVersion(InputMapDefinition::BindingVersion);
			std::string device = ToString(binding.Device);
			std::string code = binding.Code >= 0 ? GetInputCodeName(binding.Device, binding.Code) : binding.UnknownCode;
			std::string component = ToString(binding.Component);
			archive.Field("Device", device);
			archive.Field("Code", code);
			archive.Field("Component", component);
			archive.Field("Scale", binding.Scale);
			archive.Field("DeadZone", binding.DeadZone);
			if (archive.IsLoading()) {
				if (!ParseInputDevice(device, binding.Device) && warnings) {
					warnings->push_back("Unknown input device '" + device + "' (read as Key).");
				}
				ParseInputComponent(component, binding.Component);
				binding.Code = ParseInputCode(binding.Device, code);
				binding.UnknownCode = binding.Code < 0 ? code : std::string{};
				if (binding.Code < 0 && !code.empty() && warnings) {
					warnings->push_back("Unknown " + std::string(ToString(binding.Device)) + " code '" + code + "' (kept, does nothing).");
				}
			}
		}
	}

	// ---- Names ------------------------------------------------------------------------------------

	const char* ToString(InputActionType type)
	{
		switch (type) {
		case InputActionType::Button: return "Button";
		case InputActionType::Axis: return "Axis";
		case InputActionType::Vector2: return "Vector2";
		}
		return "Button";
	}

	const char* ToString(InputDevice device)
	{
		switch (device) {
		case InputDevice::Key: return "Key";
		case InputDevice::MouseButton: return "MouseButton";
		case InputDevice::MouseWheel: return "MouseWheel";
		case InputDevice::GamepadButton: return "GamepadButton";
		case InputDevice::GamepadAxis: return "GamepadAxis";
		}
		return "Key";
	}

	const char* ToString(InputComponent component)
	{
		return component == InputComponent::Y ? "Y" : "X";
	}

	bool ParseInputActionType(std::string_view text, InputActionType& outType)
	{
		for (const InputActionType type : { InputActionType::Button, InputActionType::Axis, InputActionType::Vector2 }) {
			if (text == ToString(type)) {
				outType = type;
				return true;
			}
		}
		return false;
	}

	bool ParseInputDevice(std::string_view text, InputDevice& outDevice)
	{
		for (const InputDevice device : { InputDevice::Key, InputDevice::MouseButton, InputDevice::MouseWheel, InputDevice::GamepadButton, InputDevice::GamepadAxis }) {
			if (text == ToString(device)) {
				outDevice = device;
				return true;
			}
		}
		return false;
	}

	bool ParseInputComponent(std::string_view text, InputComponent& outComponent)
	{
		if (text == "X" || text == "Y") {
			outComponent = text == "Y" ? InputComponent::Y : InputComponent::X;
			return true;
		}
		return false;
	}

	std::string GetInputCodeName(InputDevice device, int32_t code)
	{
		if (code < 0) {
			return {};
		}
		const char* name = nullptr;
		switch (device) {
		case InputDevice::Key:
			name = code < SDL_SCANCODE_COUNT ? SDL_GetScancodeName(static_cast<SDL_Scancode>(code)) : nullptr;
			break;
		case InputDevice::MouseButton:
			name = code >= 1 && code <= 5 ? MouseButtonNames[code] : nullptr;
			break;
		case InputDevice::MouseWheel:
			name = code == 0 ? "X" : (code == 1 ? "Y" : nullptr);
			break;
		case InputDevice::GamepadButton:
			name = code < SDL_GAMEPAD_BUTTON_COUNT ? SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(code)) : nullptr;
			break;
		case InputDevice::GamepadAxis:
			name = code < SDL_GAMEPAD_AXIS_COUNT ? SDL_GetGamepadStringForAxis(static_cast<SDL_GamepadAxis>(code)) : nullptr;
			break;
		}
		return name ? name : "";
	}

	int32_t ParseInputCode(InputDevice device, std::string_view name)
	{
		if (name.empty()) {
			return -1;
		}
		const std::string text{ name };
		switch (device) {
		case InputDevice::Key: {
			const SDL_Scancode key = SDL_GetScancodeFromName(text.c_str());
			return key == SDL_SCANCODE_UNKNOWN ? -1 : static_cast<int32_t>(key);
		}
		case InputDevice::MouseButton:
			for (int32_t button = 1; button <= 5; ++button) {
				if (name == MouseButtonNames[button]) {
					return button;
				}
			}
			return -1;
		case InputDevice::MouseWheel:
			return name == "X" ? 0 : (name == "Y" ? 1 : -1);
		case InputDevice::GamepadButton:
			return static_cast<int32_t>(SDL_GetGamepadButtonFromString(text.c_str())); // SDL_GAMEPAD_BUTTON_INVALID = -1
		case InputDevice::GamepadAxis:
			return static_cast<int32_t>(SDL_GetGamepadAxisFromString(text.c_str()));   // SDL_GAMEPAD_AXIS_INVALID = -1
		}
		return -1;
	}

	// ---- Definition (file) ----------------------------------------------------------------------------

	int InputMapDefinition::FindAction(std::string_view name) const
	{
		for (size_t index = 0; index < Actions.size(); ++index) {
			if (Actions[index].Name == name) {
				return static_cast<int>(index);
			}
		}
		return -1;
	}

	bool InputMapDefinition::Load(const std::filesystem::path& file, std::vector<std::string>& outWarnings, std::string& outError)
	{
		TextDocument document;
		if (!document.Load(file, outError)) {
			return false;
		}
		TextSection* header = document.FindSection(MapSection);
		if (!header) {
			outError = "Not an input map (no [InputMap] section).";
			return false;
		}
		TextArchive headerArchive(*header, ArchiveMode::Load);
		headerArchive.SerializeVersion(Version);

		Actions.clear();
		for (TextSection& section : document.Sections) {
			if (section.Type == ActionSection) {
				TextArchive archive(section, ArchiveMode::Load);
				archive.SerializeVersion(ActionVersion);
				InputAction action;
				std::string type = ToString(action.Type);
				archive.Field("Name", action.Name);
				archive.Field("Type", type);
				if (!ParseInputActionType(type, action.Type)) {
					outWarnings.push_back("Action '" + action.Name + "': unknown type '" + type + "' (read as Button).");
				}
				Actions.push_back(std::move(action));
				for (const std::string& warning : archive.GetWarnings()) {
					outWarnings.push_back(warning);
				}
			} else if (section.Type == BindingSection) {
				if (Actions.empty()) {
					outWarnings.push_back("A [Binding] before any [Action] is ignored.");
					continue;
				}
				TextArchive archive(section, ArchiveMode::Load);
				InputBinding binding;
				SerializeBinding(archive, binding, &outWarnings);
				Actions.back().Bindings.push_back(std::move(binding));
			}
		}
		return true;
	}

	bool InputMapDefinition::Save(const std::filesystem::path& file, std::string& outError) const
	{
		TextDocument document;
		TextArchive headerArchive(document.AddSection(MapSection), ArchiveMode::Save);
		headerArchive.SerializeVersion(Version);
		for (const InputAction& constAction : Actions) {
			InputAction action = constAction;
			TextArchive archive(document.AddSection(ActionSection), ArchiveMode::Save);
			archive.SerializeVersion(ActionVersion);
			std::string type = ToString(action.Type);
			archive.Field("Name", action.Name);
			archive.Field("Type", type);
			for (InputBinding& binding : action.Bindings) {
				TextArchive bindingArchive(document.AddSection(BindingSection), ArchiveMode::Save);
				SerializeBinding(bindingArchive, binding, nullptr);
			}
		}
		return document.Save(file, outError);
	}

	// ---- Asset ------------------------------------------------------------------------------------

	InputMap::InputMap()
	{
		LiveMaps().push_back(this);
	}

	InputMap::InputMap(InputMapDefinition definition)
		: Definition{ std::move(definition) }
	{
		LiveMaps().push_back(this);
	}

	InputMap::~InputMap()
	{
		std::erase(LiveMaps(), this);
	}

	std::shared_ptr<InputMap> InputMap::LoadFromFile(const std::filesystem::path& file, std::string& outError)
	{
		InputMapDefinition definition;
		std::vector<std::string> warnings;
		if (!definition.Load(file, warnings, outError)) {
			return nullptr;
		}
		for (const std::string& warning : warnings) {
			SDL_Log("%s: %s", PathToUtf8(file.filename()).c_str(), warning.c_str());
		}
		return std::make_shared<InputMap>(std::move(definition));
	}

	size_t InputMap::GetMemorySize() const
	{
		size_t bytes = sizeof(*this);
		for (const InputAction& action : Definition.Actions) {
			bytes += sizeof(InputAction) + action.Name.capacity() + action.Bindings.capacity() * sizeof(InputBinding);
		}
		return bytes;
	}

	void InputMap::SetDefinition(InputMapDefinition definition)
	{
		Definition = std::move(definition);
		for (PlayerStates& player : Players) {
			player.Actions.clear();
			player.Frame = 0;
		}
	}

	const InputActionState& InputMap::GetState(std::string_view action, uint32_t player) const
	{
		return GetState(FindAction(action), player);
	}

	const InputActionState& InputMap::GetState(int actionIndex, uint32_t player) const
	{
		if (actionIndex < 0 || actionIndex >= static_cast<int>(Definition.Actions.size()) || player >= Input::MaxPlayers) {
			return EmptyState;
		}
		PlayerStates& states = Players[player];
		const uint64_t frame = Input::GetFrame();
		if (!states.bInUse || states.Frame != frame || states.Actions.size() != Definition.Actions.size()) {
			// First question about this player (no edges yet), or asked before Input::Update this frame.
			Evaluate(player, frame, !states.bInUse || states.Actions.size() != Definition.Actions.size());
			states.bInUse = true;
		}
		return states.Actions[actionIndex];
	}

	void InputMap::UpdateAll(uint64_t frame)
	{
		for (const InputMap* map : LiveMaps()) {
			for (uint32_t player = 0; player < Input::MaxPlayers; ++player) {
				const PlayerStates& states = map->Players[player];
				if (states.bInUse && states.Frame != frame) {
					map->Evaluate(player, frame, states.Actions.size() != map->Definition.Actions.size());
				}
			}
		}
	}

	void InputMap::Evaluate(uint32_t player, uint64_t frame, bool bFirstTime) const
	{
		PlayerStates& states = Players[player];
		states.Actions.resize(Definition.Actions.size());
		states.Frame = frame;

		for (size_t index = 0; index < Definition.Actions.size(); ++index) {
			const InputAction& action = Definition.Actions[index];
			InputActionState& state = states.Actions[index];
			const bool bWasActive = state.bIsActive;

			Vector2 value;
			bool bTapped = false; // a button went down this frame (maybe up again already)
			for (const InputBinding& binding : action.Bindings) {
				const BindingValue read = Read(binding, player);
				const float contribution = read.Value * binding.Scale;
				bTapped = bTapped || (read.bPressedThisFrame && binding.Scale > 0.0f);
				switch (action.Type) {
				case InputActionType::Button:
					value.x = std::max(value.x, contribution);
					break;
				case InputActionType::Axis:
					value.x += contribution;
					break;
				case InputActionType::Vector2:
					(binding.Component == InputComponent::Y ? value.y : value.x) += contribution;
					break;
				}
			}

			bool bActive = false;
			switch (action.Type) {
			case InputActionType::Button:
				bActive = value.x >= ButtonThreshold;
				value = Vector2(bActive ? 1.0f : 0.0f, 0.0f);
				break;
			case InputActionType::Axis:
				value.x = std::clamp(value.x, -1.0f, 1.0f);
				bActive = value.x != 0.0f;
				break;
			case InputActionType::Vector2: {
				const float length = value.Length();
				if (length > 1.0f) {
					value = value * (1.0f / length);
				}
				bActive = length > 0.0f;
				break;
			}
			}

			state.Value = value;
			state.bIsActive = bActive;
			if (bFirstTime) {
				state.bPressed = false;
				state.bReleased = false;
			} else {
				const bool bTap = action.Type == InputActionType::Button && bTapped && !bWasActive && !bActive;
				state.bPressed = (bActive && !bWasActive) || bTap;
				state.bReleased = (!bActive && bWasActive) || bTap;
			}
		}
	}
}
