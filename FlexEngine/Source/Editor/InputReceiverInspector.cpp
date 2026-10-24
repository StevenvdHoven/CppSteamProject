#include "Editor/InputReceiverInspector.h"
#include "Editor/InspectorArchive.h"
#include "Editor/InspectorWindow.h"
#include "Core/Input/InputHandlerRegistery.h"
#include "Core/Input/InputMap.h"
#include "Core/Input/InputReceiver.h"

#include <imgui.h>

#include <algorithm>

namespace Flex
{
	namespace
	{
		const ImVec4 MissingColor{ 1.0f, 0.45f, 0.45f, 1.0f };

		void BeginCustomRow(const char* label)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);
			ImGui::TableSetColumnIndex(1);
		}

		// One handler binding: trigger combo, handler combo, remove. Returns true when it changed;
		// bRemove is set when it should go.
		bool DrawBindingRow(InputHandlerBinding& binding, const std::vector<InputHandlerInfo>& handlers, bool& bRemove)
		{
			bool bChanged = false;
			ImGui::SetNextItemWidth(80.0f);
			int trigger = static_cast<int>(binding.Trigger);
			if (ImGui::Combo("##Trigger", &trigger, "Pressed\0Released\0Held\0Always\0")) {
				binding.Trigger = static_cast<InputTrigger>(trigger);
				bChanged = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Pressed / Released: the frame it changes. Held: every frame while active. Always: every frame, also at 0.");
			}
			ImGui::SameLine();
			const bool bExists = std::any_of(handlers.begin(), handlers.end(), [&](const InputHandlerInfo& handler) { return handler.Name == binding.Handler; });
			const std::string preview = binding.Handler.empty() ? "(choose a handler)" : (bExists ? binding.Handler : binding.Handler + " (missing)");
			ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
			if (!bExists && !binding.Handler.empty()) {
				ImGui::PushStyleColor(ImGuiCol_Text, MissingColor);
			}
			const bool bOpen = ImGui::BeginCombo("##Handler", preview.c_str());
			if (!bExists && !binding.Handler.empty()) {
				ImGui::PopStyleColor();
			}
			if (bOpen) {
				if (handlers.empty()) {
					ImGui::TextDisabled("No handlers: write one with FLEX_INPUT_HANDLER in game code.");
				}
				for (const InputHandlerInfo& handler : handlers) {
					if (ImGui::Selectable(handler.Name.c_str(), handler.Name == binding.Handler)) {
						binding.Handler = handler.Name;
						bChanged = true;
					}
				}
				ImGui::EndCombo();
			}
			if (!bExists && !binding.Handler.empty() && ImGui::IsItemHovered()) {
				ImGui::SetTooltip("No FLEX_INPUT_HANDLER is called '%s' (is the game module built?). It's kept and runs once it exists.", binding.Handler.c_str());
			}
			ImGui::SameLine();
			if (ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0.0f))) {
				bRemove = true;
			}
			return bChanged;
		}

		void DrawInputReceiver(InspectorArchive& archive, void* component)
		{
			InputReceiver& receiver = *static_cast<InputReceiver*>(component);
			archive.Field("Map", receiver.Map, { InputMap::TypeName });
			archive.Field("Player", receiver.PlayerIndex);
			const int32_t clamped = std::clamp(receiver.PlayerIndex, 0, static_cast<int32_t>(Input::MaxPlayers) - 1);
			if (clamped != receiver.PlayerIndex) {
				receiver.PlayerIndex = clamped;
				archive.MarkEdited();
			}

			const InputMap* map = receiver.GetMap();
			if (!map) {
				BeginCustomRow("Actions");
				ImGui::TextDisabled(receiver.Map.IsValid() ? "The input map doesn't load." : "Pick an Input Map to bind handlers to its actions.");
			}
			const std::vector<InputHandlerInfo> handlers = InputHandlerRegistery::GetHandlers();
			bool bEdited = false;
			int removeIndex = -1;

			// One row per action of the map, with its handlers.
			if (map) {
				for (const InputAction& action : map->GetDefinition().Actions) {
					ImGui::PushID(action.Name.c_str());
					BeginCustomRow(action.Name.c_str());
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("%s action", ToString(action.Type));
					}
					for (size_t index = 0; index < receiver.Bindings.size(); ++index) {
						InputHandlerBinding& binding = receiver.Bindings[index];
						if (binding.Action != action.Name) {
							continue;
						}
						ImGui::PushID(static_cast<int>(index));
						bool bRemove = false;
						bEdited |= DrawBindingRow(binding, handlers, bRemove);
						if (bRemove) {
							removeIndex = static_cast<int>(index);
						}
						ImGui::PopID();
					}
					if (ImGui::SmallButton("+ Handler")) {
						InputHandlerBinding binding;
						binding.Action = action.Name;
						binding.Trigger = action.Type == InputActionType::Button ? InputTrigger::Pressed : InputTrigger::Always;
						receiver.Bindings.push_back(std::move(binding));
						bEdited = true;
					}
					ImGui::PopID();
				}
			}

			// Bindings to actions the map doesn't have (renamed, removed, or no map): kept until removed.
			bool bHeader = false;
			for (size_t index = 0; index < receiver.Bindings.size(); ++index) {
				InputHandlerBinding& binding = receiver.Bindings[index];
				if (map && map->FindAction(binding.Action) >= 0) {
					continue;
				}
				if (!bHeader) {
					BeginCustomRow("Other");
					ImGui::TextColored(MissingColor, map ? "Actions the map doesn't have:" : "Handler bindings:");
					bHeader = true;
				}
				ImGui::PushID(static_cast<int>(index + 100000));
				ImGui::TextColored(MissingColor, "%s", binding.Action.c_str());
				bool bRemove = false;
				bEdited |= DrawBindingRow(binding, handlers, bRemove);
				if (bRemove) {
					removeIndex = static_cast<int>(index);
				}
				ImGui::PopID();
			}

			if (removeIndex >= 0) {
				receiver.Bindings.erase(receiver.Bindings.begin() + removeIndex);
				bEdited = true;
			}
			if (bEdited) {
				archive.MarkEdited();
			}
		}
	}

	void RegisterInputReceiverInspector()
	{
		InspectorWindow::RegisterComponentDrawer(InputReceiver::ComponentName, &DrawInputReceiver);
	}
}
