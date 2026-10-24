#include "Editor/InputMapEditor.h"
#include "Editor/AssetDatabase.h"
#include "Editor/EditorContext.h"
#include "Core/Assets/AssetManager.h"
#include "Core/FileSystem.h"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace Flex
{
	namespace
	{
		constexpr const char* UnsavedPopup = "Unsaved input map changes";
		constexpr const char* ListenPopup = "Listen for input";
		constexpr const char* NewMapPopup = "New Input Map";
		constexpr float ListenSeconds = 10.0f;
		constexpr float EdgeFlashSeconds = 0.35f;

		const ImVec4 ErrorColor{ 1.0f, 0.4f, 0.4f, 1.0f };
		const ImVec4 WarningColor{ 1.0f, 0.8f, 0.3f, 1.0f };
		const ImVec4 ActiveColor{ 0.45f, 0.85f, 0.45f, 1.0f };

		int ListeningEditors = 0;

		// New Input Map dialog (one at a time)
		bool bRequestNewMap = false;
		std::filesystem::path NewMapFolder;
		char NewMapName[64]{};
		std::string NewMapError;

		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
			return text;
		}

		InputBinding MakeBinding(InputDevice device, int32_t code, InputComponent component = InputComponent::X, float scale = 1.0f)
		{
			InputBinding binding;
			binding.Device = device;
			binding.Code = code;
			binding.Component = component;
			binding.Scale = scale;
			return binding;
		}

		// Every code a device has, with its name (for the code combo).
		std::vector<std::pair<int32_t, std::string>> ListCodes(InputDevice device)
		{
			std::vector<std::pair<int32_t, std::string>> codes;
			int32_t count = 0;
			switch (device) {
			case InputDevice::Key: count = SDL_SCANCODE_COUNT; break;
			case InputDevice::MouseButton: count = 6; break;
			case InputDevice::MouseWheel: count = 2; break;
			case InputDevice::GamepadButton: count = SDL_GAMEPAD_BUTTON_COUNT; break;
			case InputDevice::GamepadAxis: count = SDL_GAMEPAD_AXIS_COUNT; break;
			}
			for (int32_t code = 0; code < count; ++code) {
				std::string name = GetInputCodeName(device, code);
				if (!name.empty()) {
					codes.emplace_back(code, std::move(name));
				}
			}
			return codes;
		}

		// A -1..1 value as a bar that fills from the middle.
		void CenteredBar(float value, const char* label)
		{
			const ImVec2 size(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight());
			const ImVec2 min = ImGui::GetCursorScreenPos();
			const ImVec2 max(min.x + size.x, min.y + size.y);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBg), 2.0f);
			const float middle = (min.x + max.x) * 0.5f;
			const float end = middle + std::clamp(value, -1.0f, 1.0f) * (size.x * 0.5f);
			drawList->AddRectFilled(ImVec2(std::min(middle, end), min.y), ImVec2(std::max(middle, end), max.y), ImGui::GetColorU32(ImGuiCol_PlotHistogram), 2.0f);
			drawList->AddLine(ImVec2(middle, min.y), ImVec2(middle, max.y), ImGui::GetColorU32(ImGuiCol_Border));
			const ImVec2 textSize = ImGui::CalcTextSize(label);
			drawList->AddText(ImVec2(middle - textSize.x * 0.5f, min.y + (size.y - textSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), label);
			ImGui::Dummy(size);
		}

		bool IsValidName(const std::string& name)
		{
			if (name.empty()) {
				return false;
			}
			return std::all_of(name.begin(), name.end(), [](unsigned char character) { return std::isalnum(character) || character == '_' || character == ' ' || character == '-'; });
		}
	}

	InputMapEditor::InputMapEditor(EditorContext& context, Uid inputMapId)
		: AssetEditor("Input Map", "InputMapEditor", inputMapId)
		, Context{ context }
		, Database{ context.Assets }
		, InputMapId{ inputMapId }
	{
		Load();
	}

	InputMapEditor::~InputMapEditor()
	{
		StopListening();
	}

	bool InputMapEditor::IsListening()
	{
		return ListeningEditors > 0;
	}

	InputMapDefinition InputMapEditor::MakeDefaultDefinition()
	{
		InputMapDefinition definition;
		InputAction move{ "Move", InputActionType::Vector2, {} };
		move.Bindings = {
			MakeBinding(InputDevice::Key, SDL_SCANCODE_A, InputComponent::X, -1.0f), MakeBinding(InputDevice::Key, SDL_SCANCODE_D, InputComponent::X, 1.0f),
			MakeBinding(InputDevice::Key, SDL_SCANCODE_W, InputComponent::Y, -1.0f), MakeBinding(InputDevice::Key, SDL_SCANCODE_S, InputComponent::Y, 1.0f),
			MakeBinding(InputDevice::Key, SDL_SCANCODE_LEFT, InputComponent::X, -1.0f), MakeBinding(InputDevice::Key, SDL_SCANCODE_RIGHT, InputComponent::X, 1.0f),
			MakeBinding(InputDevice::Key, SDL_SCANCODE_UP, InputComponent::Y, -1.0f), MakeBinding(InputDevice::Key, SDL_SCANCODE_DOWN, InputComponent::Y, 1.0f),
			MakeBinding(InputDevice::GamepadAxis, SDL_GAMEPAD_AXIS_LEFTX, InputComponent::X), MakeBinding(InputDevice::GamepadAxis, SDL_GAMEPAD_AXIS_LEFTY, InputComponent::Y),
			MakeBinding(InputDevice::GamepadButton, SDL_GAMEPAD_BUTTON_DPAD_LEFT, InputComponent::X, -1.0f), MakeBinding(InputDevice::GamepadButton, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, InputComponent::X, 1.0f),
			MakeBinding(InputDevice::GamepadButton, SDL_GAMEPAD_BUTTON_DPAD_UP, InputComponent::Y, -1.0f), MakeBinding(InputDevice::GamepadButton, SDL_GAMEPAD_BUTTON_DPAD_DOWN, InputComponent::Y, 1.0f),
		};
		InputAction jump{ "Jump", InputActionType::Button, { MakeBinding(InputDevice::Key, SDL_SCANCODE_SPACE), MakeBinding(InputDevice::GamepadButton, SDL_GAMEPAD_BUTTON_SOUTH) } };
		definition.Actions = { std::move(move), std::move(jump) };
		return definition;
	}

	// ---- Loading and saving ---------------------------------------------------------------------

	void InputMapEditor::Load()
	{
		ErrorMessage.clear();
		Warnings.clear();
		const AssetInfo* asset = Database.Find(InputMapId);
		if (!asset) {
			ErrorMessage = "This input map no longer exists.";
			return;
		}
		SetTitle(asset->Name + " (Input Map)");
		std::string error;
		if (!Edit.Load(Database.GetSourcePath(*asset), Warnings, error)) {
			ErrorMessage = error;
		}
		bDirty = false;
		SelectedAction = Edit.Actions.empty() ? -1 : 0;
		Preview = std::make_unique<InputMap>(Edit);
	}

	bool InputMapEditor::Save()
	{
		const AssetInfo* asset = Database.Find(InputMapId);
		if (!asset) {
			ErrorMessage = "This input map no longer exists.";
			return false;
		}
		std::string error;
		if (!Edit.Save(Database.GetSourcePath(*asset), error)) {
			ErrorMessage = "Could not save: " + error;
			return false;
		}
		// Every user of the loaded asset (a running game too) sees the new actions at once.
		if (std::shared_ptr<InputMap> loaded = std::dynamic_pointer_cast<InputMap>(AssetManager::FindLoaded(InputMapId))) {
			loaded->SetDefinition(Edit);
		}
		ErrorMessage.clear();
		bDirty = false;
		return true;
	}

	void InputMapEditor::Changed()
	{
		bDirty = true;
		Preview->SetDefinition(Edit);
	}

	// ---- Window ----------------------------------------------------------------------------------

	void InputMapEditor::Draw()
	{
		ImGui::SetNextWindowSize(ImVec2(1150.0f, 600.0f), ImGuiCond_FirstUseEver);
		const bool bVisible = BeginWindow();

		// The tab's close button: keep the window open and ask when there are unsaved changes.
		if (!bIsOpen && bDirty) {
			bIsOpen = true;
			bRequestUnsavedPopup = true;
		}
		if (!bIsOpen) {
			StopListening();
		}
		if (!bVisible) {
			EndWindow();
			return;
		}

		// Keyboard and mouse reach Input (for Listen and the preview) while this window has focus.
		const bool bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (bListening || (bFocused && !ImGui::GetIO().WantTextInput)) {
			Context.bGameHasInputFocus = true;
		}
		if (bFocused && !bListening && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteFocused)) {
			Save();
		}

		if (bRequestUnsavedPopup) {
			ImGui::OpenPopup(UnsavedPopup);
			bRequestUnsavedPopup = false;
		}
		DrawUnsavedChangesPopup();
		if (bRequestListenPopup) {
			ImGui::OpenPopup(ListenPopup);
			bRequestListenPopup = false;
		}
		DrawListenPopup();

		const AssetInfo* asset = Database.Find(InputMapId);
		if (!asset) {
			ImGui::TextDisabled("This input map no longer exists.");
			EndWindow();
			return;
		}

		DrawToolbar();
		ImGui::Separator();
		if (ImGui::BeginTable("InputMapLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 170.0f);
			ImGui::TableSetupColumn("Bindings", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 210.0f);
			ImGui::TableNextColumn();
			DrawActions();
			ImGui::TableNextColumn();
			DrawActionDetails();
			ImGui::TableNextColumn();
			DrawPreview();
			ImGui::EndTable();
		}
		EndWindow();
	}

	void InputMapEditor::DrawToolbar()
	{
		const AssetInfo* asset = Database.Find(InputMapId);
		ImGui::Text("%s", PathToGenericUtf8(asset->Path).c_str());
		ImGui::SameLine();
		ImGui::BeginDisabled(!bDirty);
		if (ImGui::Button("Save")) {
			Save();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!bDirty);
		if (ImGui::Button("Revert")) {
			Load();
		}
		ImGui::EndDisabled();
		if (bDirty) {
			ImGui::SameLine();
			ImGui::TextColored(WarningColor, "(unsaved, Ctrl+S)");
		}
		if (!ErrorMessage.empty()) {
			ImGui::TextColored(ErrorColor, "%s", ErrorMessage.c_str());
		}
		for (const std::string& warning : Warnings) {
			ImGui::TextColored(WarningColor, "%s", warning.c_str());
		}
	}

	void InputMapEditor::DrawActions()
	{
		ImGui::TextDisabled("Actions");
		if (ImGui::BeginChild("ActionList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() * 2.0f))) {
			for (size_t index = 0; index < Edit.Actions.size(); ++index) {
				const InputAction& action = Edit.Actions[index];
				ImGui::PushID(static_cast<int>(index));
				const std::string label = action.Name + "  (" + ToString(action.Type) + ")";
				if (ImGui::Selectable(label.c_str(), SelectedAction == static_cast<int>(index))) {
					SelectedAction = static_cast<int>(index);
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		if (ImGui::Button("Add")) {
			std::string name = "Action";
			for (int number = 2; Edit.FindAction(name) >= 0; ++number) {
				name = "Action" + std::to_string(number);
			}
			Edit.Actions.push_back(InputAction{ name, InputActionType::Button, {} });
			SelectedAction = static_cast<int>(Edit.Actions.size()) - 1;
			Changed();
		}
		const bool bHasSelection = SelectedAction >= 0 && SelectedAction < static_cast<int>(Edit.Actions.size());
		ImGui::SameLine();
		ImGui::BeginDisabled(!bHasSelection);
		if (ImGui::Button("Delete")) {
			Edit.Actions.erase(Edit.Actions.begin() + SelectedAction);
			SelectedAction = std::min(SelectedAction, static_cast<int>(Edit.Actions.size()) - 1);
			Changed();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!bHasSelection || SelectedAction == 0);
		if (ImGui::ArrowButton("Up", ImGuiDir_Up)) {
			std::swap(Edit.Actions[SelectedAction], Edit.Actions[SelectedAction - 1]);
			--SelectedAction;
			Changed();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!bHasSelection || SelectedAction + 1 >= static_cast<int>(Edit.Actions.size()));
		if (ImGui::ArrowButton("Down", ImGuiDir_Down)) {
			std::swap(Edit.Actions[SelectedAction], Edit.Actions[SelectedAction + 1]);
			++SelectedAction;
			Changed();
		}
		ImGui::EndDisabled();
	}

	void InputMapEditor::DrawActionDetails()
	{
		if (SelectedAction < 0 || SelectedAction >= static_cast<int>(Edit.Actions.size())) {
			ImGui::TextDisabled("Select or add an action.");
			return;
		}
		InputAction& action = Edit.Actions[SelectedAction];

		char name[64];
		std::snprintf(name, sizeof(name), "%s", action.Name.c_str());
		ImGui::SetNextItemWidth(200.0f);
		if (ImGui::InputText("Name", name, sizeof(name), ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit()) {
			const int existing = Edit.FindAction(name);
			if (IsValidName(name) && (existing < 0 || existing == SelectedAction) && action.Name != name) {
				action.Name = name;
				Changed();
			}
		}
		if (Edit.FindAction(action.Name) != SelectedAction) {
			ImGui::TextColored(ErrorColor, "Two actions are called '%s'.", action.Name.c_str());
		}
		ImGui::SetNextItemWidth(200.0f);
		int type = static_cast<int>(action.Type);
		if (ImGui::Combo("Type", &type, "Button\0Axis\0Vector2\0")) {
			action.Type = static_cast<InputActionType>(type);
			Changed();
		}
		ImGui::SameLine();
		ImGui::TextDisabled(action.Type == InputActionType::Button ? "(on/off)" : action.Type == InputActionType::Axis ? "(-1..1)" : "(x, y; length up to 1)");

		ImGui::Separator();
		const bool bVector = action.Type == InputActionType::Vector2;
		const int columns = 6;
		if (ImGui::BeginTable("Bindings", columns, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthFixed, 110.0f);
			ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(bVector ? "Adds to" : "", ImGuiTableColumnFlags_WidthFixed, bVector ? 40.0f : 1.0f);
			ImGui::TableSetupColumn("Scale", ImGuiTableColumnFlags_WidthFixed, 50.0f);
			ImGui::TableSetupColumn("Dead zone", ImGuiTableColumnFlags_WidthFixed, 70.0f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f);
			ImGui::TableHeadersRow();
			for (size_t index = 0; index < action.Bindings.size(); ++index) {
				DrawBinding(action, index);
			}
			ImGui::EndTable();
		}
		if (ImGui::Button("Add binding")) {
			action.Bindings.push_back(InputBinding{});
			Changed();
		}
		ImGui::SameLine();
		if (ImGui::Button("Listen for a new binding...")) {
			StartListening(-1);
		}
		if (bVector) {
			ImGui::TextDisabled("WASD-style: one binding per direction, Scale -1 for left/up (y points down).");
		}
	}

	void InputMapEditor::DrawBinding(InputAction& action, size_t index)
	{
		InputBinding& binding = action.Bindings[index];
		ImGui::PushID(static_cast<int>(index));
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1.0f);
		int device = static_cast<int>(binding.Device);
		if (ImGui::Combo("##Device", &device, "Key\0Mouse button\0Mouse wheel\0Gamepad button\0Gamepad axis\0")) {
			binding.Device = static_cast<InputDevice>(device);
			binding.Code = -1;
			binding.UnknownCode.clear();
			Changed();
		}

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1.0f);
		const std::string current = binding.Code >= 0 ? GetInputCodeName(binding.Device, binding.Code) : (binding.UnknownCode.empty() ? "(choose)" : binding.UnknownCode + " (unknown)");
		if (ImGui::BeginCombo("##Code", current.c_str(), ImGuiComboFlags_HeightLarge)) {
			if (ImGui::IsWindowAppearing()) {
				CodeFilter[0] = '\0';
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##Filter", "Search", CodeFilter, sizeof(CodeFilter));
			const std::string filter = ToLower(CodeFilter);
			for (const auto& [code, name] : ListCodes(binding.Device)) {
				if (!filter.empty() && ToLower(name).find(filter) == std::string::npos) {
					continue;
				}
				if (ImGui::Selectable(name.c_str(), code == binding.Code)) {
					binding.Code = code;
					binding.UnknownCode.clear();
					Changed();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::TableNextColumn();
		if (action.Type == InputActionType::Vector2) {
			ImGui::SetNextItemWidth(-1.0f);
			int component = static_cast<int>(binding.Component);
			if (ImGui::Combo("##Component", &component, "X\0Y\0")) {
				binding.Component = static_cast<InputComponent>(component);
				Changed();
			}
		}

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat("##Scale", &binding.Scale, 0.05f, -10.0f, 10.0f, "%.2f")) {
			Changed();
		}

		ImGui::TableNextColumn();
		if (binding.Device == InputDevice::GamepadAxis) {
			bool bOwn = binding.DeadZone >= 0.0f;
			if (ImGui::Checkbox("##OwnDeadZone", &bOwn)) {
				binding.DeadZone = bOwn ? Input::GetDeadZone() : -1.0f;
				Changed();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Off: Input's dead zone (%.2f). On: this binding's own.", Input::GetDeadZone());
			}
			if (bOwn) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::DragFloat("##DeadZone", &binding.DeadZone, 0.01f, 0.0f, 0.95f, "%.2f")) {
					Changed();
				}
			}
		}

		ImGui::TableNextColumn();
		if (ImGui::SmallButton("Listen")) {
			StartListening(static_cast<int>(index));
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("x")) {
			action.Bindings.erase(action.Bindings.begin() + static_cast<std::ptrdiff_t>(index));
			Changed();
		}
		ImGui::PopID();
	}

	void InputMapEditor::DrawPreview()
	{
		ImGui::TextDisabled("Preview");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderInt("Player", &PreviewPlayer, 0, static_cast<int>(Input::MaxPlayers) - 1);
		if (Input::IsGamepadConnected(static_cast<uint32_t>(PreviewPlayer))) {
			ImGui::TextDisabled("%s", Input::GetGamepadName(static_cast<uint32_t>(PreviewPlayer)).c_str());
		} else {
			ImGui::TextDisabled(PreviewPlayer == 0 ? "Keyboard + mouse (no gamepad)" : "No gamepad");
		}
		ImGui::TextDisabled("Keyboard/mouse: while this window has focus.");
		ImGui::Separator();

		const auto now = std::chrono::steady_clock::now();
		LastPressed.resize(Edit.Actions.size());
		LastReleased.resize(Edit.Actions.size());
		for (size_t index = 0; index < Edit.Actions.size(); ++index) {
			const InputAction& action = Edit.Actions[index];
			const InputActionState& state = Preview->GetState(static_cast<int>(index), static_cast<uint32_t>(PreviewPlayer));
			if (state.bPressed) {
				LastPressed[index] = now;
			}
			if (state.bReleased) {
				LastReleased[index] = now;
			}
			ImGui::PushID(static_cast<int>(index));
			if (state.bIsActive) {
				ImGui::TextColored(ActiveColor, "%s", action.Name.c_str());
			} else {
				ImGui::TextUnformatted(action.Name.c_str());
			}
			ImGui::SameLine();
			if (std::chrono::duration<float>(now - LastPressed[index]).count() < EdgeFlashSeconds) {
				ImGui::TextColored(ActiveColor, "pressed");
				ImGui::SameLine();
			}
			if (std::chrono::duration<float>(now - LastReleased[index]).count() < EdgeFlashSeconds) {
				ImGui::TextColored(WarningColor, "released");
				ImGui::SameLine();
			}
			ImGui::NewLine();
			char text[48];
			switch (action.Type) {
			case InputActionType::Button:
				ImGui::ProgressBar(state.Value.x, ImVec2(-1.0f, 0.0f), state.bIsActive ? "held" : "");
				break;
			case InputActionType::Axis:
				std::snprintf(text, sizeof(text), "%.2f", state.Value.x);
				CenteredBar(state.Value.x, text);
				break;
			case InputActionType::Vector2:
				std::snprintf(text, sizeof(text), "x %.2f", state.Value.x);
				CenteredBar(state.Value.x, text);
				std::snprintf(text, sizeof(text), "y %.2f", state.Value.y);
				CenteredBar(state.Value.y, text);
				break;
			}
			ImGui::PopID();
		}
	}

	// ---- Listen ----------------------------------------------------------------------------------

	void InputMapEditor::StartListening(int bindingIndex)
	{
		if (!bListening) {
			++ListeningEditors;
		}
		bListening = true;
		bRequestListenPopup = true;
		ListenBinding = bindingIndex;
		ListenStart = std::chrono::steady_clock::now();
		for (uint32_t player = 0; player < Input::MaxPlayers; ++player) {
			for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis) {
				ListenAxes[player][axis] = Input::GetGamepadAxisRaw(player, static_cast<SDL_GamepadAxis>(axis));
			}
		}
	}

	void InputMapEditor::StopListening()
	{
		if (bListening) {
			--ListeningEditors;
		}
		bListening = false;
	}

	void InputMapEditor::FinishListening(const InputBinding& captured)
	{
		StopListening();
		if (SelectedAction < 0 || SelectedAction >= static_cast<int>(Edit.Actions.size())) {
			return;
		}
		InputAction& action = Edit.Actions[SelectedAction];
		if (ListenBinding >= 0 && ListenBinding < static_cast<int>(action.Bindings.size())) {
			// Only the input changes: a WASD key keeps its component and scale. A stick takes the direction it
			// was pushed (button actions) or its own component (Vector2 actions).
			InputBinding& binding = action.Bindings[ListenBinding];
			binding.Device = captured.Device;
			binding.Code = captured.Code;
			binding.UnknownCode.clear();
			if (captured.Device == InputDevice::GamepadAxis) {
				if (action.Type == InputActionType::Button) {
					binding.Scale = captured.Scale;
				} else if (action.Type == InputActionType::Vector2) {
					binding.Component = captured.Component;
				}
			}
		} else {
			action.Bindings.push_back(captured);
		}
		Changed();
	}

	void InputMapEditor::DrawListenPopup()
	{
		if (!ImGui::BeginPopupModal(ListenPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
			if (bListening && !bRequestListenPopup) {
				StopListening(); // closed some other way
			}
			return;
		}
		const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - ListenStart).count();
		ImGui::TextUnformatted("Press a key or gamepad button, or move a stick or trigger.");
		ImGui::TextDisabled("Esc cancels (%.0f s left).", std::max(0.0f, ListenSeconds - elapsed));

		// Mouse buttons and the wheel only count on this box, so clicking around doesn't bind them.
		const char* hint = "Click or scroll here to bind a mouse button or the wheel";
		const ImVec2 hintSize = ImGui::CalcTextSize(hint);
		const ImVec2 boxSize(std::max(360.0f, hintSize.x + 24.0f), 54.0f);
		const ImVec2 boxMin = ImGui::GetCursorScreenPos();
		ImGui::Dummy(boxSize);
		const bool bMouseOnBox = ImGui::IsMouseHoveringRect(boxMin, ImVec2(boxMin.x + boxSize.x, boxMin.y + boxSize.y));
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRect(boxMin, ImVec2(boxMin.x + boxSize.x, boxMin.y + boxSize.y), bMouseOnBox ? IM_COL32(120, 200, 120, 255) : IM_COL32(150, 150, 150, 160), 4.0f);
		drawList->AddText(ImVec2(boxMin.x + (boxSize.x - hintSize.x) * 0.5f, boxMin.y + (boxSize.y - hintSize.y) * 0.5f), IM_COL32(200, 200, 200, 220), hint);

		const bool bVector = SelectedAction >= 0 && SelectedAction < static_cast<int>(Edit.Actions.size()) && Edit.Actions[SelectedAction].Type == InputActionType::Vector2;
		const bool bButtonAction = SelectedAction >= 0 && SelectedAction < static_cast<int>(Edit.Actions.size()) && Edit.Actions[SelectedAction].Type == InputActionType::Button;
		bool bDone = false;
		InputBinding captured;
		if (Input::WasKeyPressed(SDL_SCANCODE_ESCAPE) || elapsed > ListenSeconds) {
			StopListening();
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			return;
		}
		for (int key = 1; key < SDL_SCANCODE_COUNT && !bDone; ++key) {
			if (Input::WasKeyPressed(static_cast<SDL_Scancode>(key))) {
				captured = MakeBinding(InputDevice::Key, key);
				bDone = true;
			}
		}
		for (uint8_t button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_X2 && !bDone && bMouseOnBox; ++button) {
			if (Input::WasMouseButtonPressed(button)) {
				captured = MakeBinding(InputDevice::MouseButton, button);
				bDone = true;
			}
		}
		if (!bDone && bMouseOnBox) {
			const Vector2 wheel = Input::GetMouseWheel();
			if (wheel.y != 0.0f || wheel.x != 0.0f) {
				captured = MakeBinding(InputDevice::MouseWheel, wheel.y != 0.0f ? 1 : 0);
				bDone = true;
			}
		}
		for (uint32_t player = 0; player < Input::MaxPlayers && !bDone; ++player) {
			for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT && !bDone; ++button) {
				if (Input::WasGamepadButtonPressed(player, static_cast<SDL_GamepadButton>(button))) {
					captured = MakeBinding(InputDevice::GamepadButton, button);
					bDone = true;
				}
			}
			for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT && !bDone; ++axis) {
				const float value = Input::GetGamepadAxisRaw(player, static_cast<SDL_GamepadAxis>(axis));
				if (std::abs(value) > 0.5f && std::abs(value - ListenAxes[player][axis]) > 0.5f) {
					// A button action follows the direction it was pushed; axes use the whole range.
					captured = MakeBinding(InputDevice::GamepadAxis, axis, InputComponent::X, bButtonAction && value < 0.0f ? -1.0f : 1.0f);
					if (bVector) {
						captured.Component = axis == SDL_GAMEPAD_AXIS_LEFTY || axis == SDL_GAMEPAD_AXIS_RIGHTY ? InputComponent::Y : InputComponent::X;
					}
					bDone = true;
				}
			}
		}
		if (bDone) {
			FinishListening(captured);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void InputMapEditor::DrawUnsavedChangesPopup()
	{
		if (!ImGui::BeginPopupModal(UnsavedPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			return;
		}
		ImGui::TextUnformatted("This input map has changes that aren't saved.");
		if (ImGui::Button("Save and Close")) {
			if (Save()) {
				bIsOpen = false;
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard and Close")) {
			bDirty = false;
			bIsOpen = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// ---- New Input Map dialog ---------------------------------------------------------------------

	void InputMapEditor::OpenNewInputMapDialog(const std::filesystem::path& folder)
	{
		bRequestNewMap = true;
		NewMapFolder = folder;
		NewMapError.clear();
		std::snprintf(NewMapName, sizeof(NewMapName), "%s", "PlayerInput");
	}

	void InputMapEditor::DrawNewInputMapDialog(EditorContext& context)
	{
		if (bRequestNewMap) {
			ImGui::OpenPopup(NewMapPopup);
			bRequestNewMap = false;
		}
		if (!ImGui::BeginPopupModal(NewMapPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			return;
		}
		ImGui::TextUnformatted("A new input map with a Move (WASD, arrows, left stick, D-pad) and a Jump action.");
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::SetNextItemWidth(260.0f);
		const bool bEnter = ImGui::InputText("Name", NewMapName, sizeof(NewMapName), ImGuiInputTextFlags_EnterReturnsTrue);
		if (!NewMapError.empty()) {
			ImGui::TextColored(ErrorColor, "%s", NewMapError.c_str());
		}
		if (ImGui::Button("Create") || bEnter) {
			const std::string name = NewMapName;
			std::error_code error;
			const std::filesystem::path file = NewMapFolder / Utf8ToPath(name + InputMap::FileExtension);
			if (!IsValidName(name) || name.find(' ') != std::string::npos) {
				NewMapError = "Use letters, digits, '_' and '-'.";
			} else if (std::filesystem::exists(file, error)) {
				NewMapError = name + InputMap::FileExtension + " already exists here.";
			} else if (std::string saveError; !MakeDefaultDefinition().Save(file, saveError)) {
				NewMapError = saveError;
			} else {
				context.RefreshAssets();
				const std::filesystem::path relativeFile = std::filesystem::relative(file, context.Assets.GetContentDirectory(), error);
				if (const AssetInfo* asset = context.Assets.FindByPath(relativeFile)) {
					context.OpenAsset(asset->Id);
				}
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
