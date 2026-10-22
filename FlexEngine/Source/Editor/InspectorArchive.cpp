#include "Editor/InspectorArchive.h"
#include "Editor/EditorContext.h"
#include "Core/FileSystem.h"
#include "Core/Math/MathUtils.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace Flex
{
	namespace
	{
		int ResizeStringCallback(ImGuiInputTextCallbackData* data)
		{
			if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
				std::string* text = static_cast<std::string*>(data->UserData);
				text->resize(static_cast<size_t>(data->BufTextLen));
				data->Buf = text->data();
			}
			return 0;
		}

		bool IsAllowed(const std::string& assetType, std::initializer_list<const char*> allowedAssetTypes)
		{
			if (allowedAssetTypes.size() == 0) {
				return true;
			}
			return std::any_of(allowedAssetTypes.begin(), allowedAssetTypes.end(), [&](const char* type) { return assetType == type; });
		}

		std::string JoinTypes(std::initializer_list<const char*> allowedAssetTypes)
		{
			std::string text;
			for (const char* type : allowedAssetTypes) {
				text += text.empty() ? type : std::string(", ") + type;
			}
			return text.empty() ? std::string("any asset") : text;
		}

		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		// Only one picker is open at a time, so one search buffer is enough.
		char PickerSearch[128]{};
	}

	InspectorArchive::InspectorArchive(EditorContext& context)
		: Archive(ArchiveMode::Load) // values are edited in place, like a load that never fails
		, Context{ context }
	{
	}

	void InspectorArchive::BeginRow(const char* name)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::Indent(static_cast<float>(Groups.size()) * ImGui::GetStyle().IndentSpacing);
		ImGui::TextUnformatted(name);
		ImGui::Unindent(static_cast<float>(Groups.size()) * ImGui::GetStyle().IndentSpacing);
		ImGui::TableNextColumn();
		ImGui::PushID(name);
		ImGui::SetNextItemWidth(-FLT_MIN);
	}

	void InspectorArchive::EndRow(bool bValueChanged)
	{
		bChanged |= bValueChanged;
		bEditStarted |= ImGui::IsItemActivated();
		bEditFinished |= ImGui::IsItemDeactivatedAfterEdit();
		ImGui::PopID();
	}

	void InspectorArchive::Field(const char* name, bool& value)
	{
		BeginRow(name);
		EndRow(ImGui::Checkbox("##Value", &value));
	}

	void InspectorArchive::Field(const char* name, int32_t& value)
	{
		BeginRow(name);
		EndRow(ImGui::DragInt("##Value", &value));
	}

	void InspectorArchive::Field(const char* name, uint32_t& value)
	{
		BeginRow(name);
		EndRow(ImGui::DragScalar("##Value", ImGuiDataType_U32, &value));
	}

	void InspectorArchive::Field(const char* name, int64_t& value)
	{
		BeginRow(name);
		EndRow(ImGui::DragScalar("##Value", ImGuiDataType_S64, &value));
	}

	void InspectorArchive::Field(const char* name, float& value)
	{
		BeginRow(name);
		if (std::strcmp(name, "Rotation") == 0) {
			float degrees = Math::RadiansToDegrees(value);
			const bool bValueChanged = ImGui::DragFloat("##Value", &degrees, 0.5f, 0.0f, 0.0f, "%.2f deg");
			if (bValueChanged) {
				value = Math::DegreesToRadians(degrees);
			}
			EndRow(bValueChanged);
			return;
		}
		EndRow(ImGui::DragFloat("##Value", &value, 0.1f));
	}

	void InspectorArchive::Field(const char* name, double& value)
	{
		BeginRow(name);
		EndRow(ImGui::DragScalar("##Value", ImGuiDataType_Double, &value, 0.1f));
	}

	void InspectorArchive::Field(const char* name, std::string& value)
	{
		BeginRow(name);
		EndRow(ImGui::InputText("##Value", value.data(), value.capacity() + 1, ImGuiInputTextFlags_CallbackResize, ResizeStringCallback, &value));
	}

	void InspectorArchive::Field(const char* name, Uid& value)
	{
		AssetSlot(name, value, {});
	}

	void InspectorArchive::Field(const char* name, Uid& value, std::initializer_list<const char*> allowedAssetTypes)
	{
		AssetSlot(name, value, allowedAssetTypes);
	}

	// An asset slot: shows the asset's name and thumbnail. Click it to pick from a searchable
	// list of the allowed asset types, drop an allowed asset from Content on it, or clear it.
	void InspectorArchive::AssetSlot(const char* name, Uid& value, std::initializer_list<const char*> allowedAssetTypes)
	{
		BeginRow(name);
		const AssetInfo* asset = value.IsValid() ? Context.Assets.Find(value) : nullptr;
		const float height = ImGui::GetFrameHeight();
		const ImVec2 thumbMin = ImGui::GetCursorScreenPos();
		if (asset) {
			Context.Thumbnails.Draw(ImGui::GetWindowDrawList(), value, thumbMin, ImVec2(thumbMin.x + height, thumbMin.y + height));
		}
		ImGui::Dummy(ImVec2(height, height));
		ImGui::SameLine();

		const std::string types = JoinTypes(allowedAssetTypes);
		const std::string label = asset ? asset->Name + "  (" + asset->Type + ")"
			: value.IsValid() ? "Missing " + value.ToString() : "None (" + types + ")";
		const float clearWidth = value.IsValid() ? height + ImGui::GetStyle().ItemSpacing.x : 0.0f;
		bool bValueChanged = false;
		if (ImGui::Button(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - clearWidth, 0.0f))) {
			PickerSearch[0] = '\0';
			ImGui::OpenPopup("##AssetPicker");
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s\nClick to pick, or drop one from Content.%s", types.c_str(), asset ? " Double-click to select it." : "");
			if (asset && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				Context.CurrentSelection.SelectAsset(value);
			}
		}

		// Only assets of an allowed type can be dropped (others don't highlight the slot).
		if (ImGui::BeginDragDropTarget()) {
			const ImGuiPayload* dragged = ImGui::GetDragDropPayload();
			if (dragged && dragged->IsDataType(AssetPayloadType)) {
				const AssetInfo* draggedAsset = Context.Assets.Find(*static_cast<const Uid*>(dragged->Data));
				if (draggedAsset && IsAllowed(draggedAsset->Type, allowedAssetTypes)) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadType)) {
						value = *static_cast<const Uid*>(payload->Data);
						bValueChanged = true;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		bValueChanged |= AssetPicker(value, allowedAssetTypes);
		if (value.IsValid()) {
			ImGui::SameLine();
			if (ImGui::Button("x", ImVec2(height, 0.0f))) {
				value = Uid();
				bValueChanged = true;
			}
		}

		// A drop or a clear is a whole edit in one frame.
		bChanged |= bValueChanged;
		bEditStarted |= bValueChanged;
		bEditFinished |= bValueChanged;
		ImGui::PopID();
	}

	bool InspectorArchive::AssetPicker(Uid& value, std::initializer_list<const char*> allowedAssetTypes)
	{
		if (!ImGui::BeginPopup("##AssetPicker")) {
			return false;
		}

		ImGui::TextDisabled("%s", JoinTypes(allowedAssetTypes).c_str());
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::SetNextItemWidth(280.0f);
		ImGui::InputTextWithHint("##Search", "Search", PickerSearch, sizeof(PickerSearch));
		const std::string search = ToLower(PickerSearch);

		bool bPicked = false;
		const float rowHeight = ImGui::GetFrameHeight();
		if (ImGui::BeginChild("##List", ImVec2(280.0f, 260.0f), ImGuiChildFlags_Borders)) {
			if (ImGui::Selectable("None", !value.IsValid())) {
				value = Uid();
				bPicked = true;
			}
			for (const AssetInfo& asset : Context.Assets.GetAssets()) {
				if (!IsAllowed(asset.Type, allowedAssetTypes) || (!search.empty() && ToLower(asset.Name).find(search) == std::string::npos)) {
					continue;
				}
				ImGui::PushID(static_cast<int>(asset.Id.GetValue() ^ (asset.Id.GetValue() >> 32)));
				const ImVec2 rowMin = ImGui::GetCursorScreenPos();
				if (ImGui::Selectable("##Asset", asset.Id == value, 0, ImVec2(0.0f, rowHeight))) {
					value = asset.Id;
					bPicked = true;
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::SetTooltip("%s", (asset.Type + " | " + PathToGenericUtf8(asset.Path)).c_str());
				}
				// Thumbnail, name and (when several types are allowed) the type.
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				Context.Thumbnails.Draw(drawList, asset.Id, rowMin, ImVec2(rowMin.x + rowHeight, rowMin.y + rowHeight));
				const float textY = rowMin.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
				drawList->AddText(ImVec2(rowMin.x + rowHeight + 6.0f, textY), ImGui::GetColorU32(ImGuiCol_Text), asset.Name.c_str());
				if (allowedAssetTypes.size() != 1) {
					const float typeWidth = ImGui::CalcTextSize(asset.Type.c_str()).x;
					drawList->AddText(ImVec2(rowMin.x + ImGui::GetContentRegionAvail().x - typeWidth, textY), ImGui::GetColorU32(ImGuiCol_TextDisabled), asset.Type.c_str());
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		if (bPicked) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
		return bPicked;
	}

	void InspectorArchive::Field(const char* name, Vector2& value)
	{
		BeginRow(name);
		EndRow(ImGui::DragFloat2("##Value", &value.x, 0.1f));
	}

	void InspectorArchive::Field(const char* name, Vector2Int& value)
	{
		BeginRow(name);
		EndRow(ImGui::DragInt2("##Value", &value.x));
	}

	void InspectorArchive::Field(const char* name, Color& value)
	{
		BeginRow(name);
		EndRow(ImGui::ColorEdit4("##Value", &value.r, ImGuiColorEditFlags_Float));
	}

	void InspectorArchive::BeginObject(const char* name)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Indent(static_cast<float>(Groups.size()) * ImGui::GetStyle().IndentSpacing);
		ImGui::TextDisabled("%s", name);
		ImGui::Unindent(static_cast<float>(Groups.size()) * ImGui::GetStyle().IndentSpacing);
		Groups.emplace_back(name);
		ImGui::PushID(name);
	}

	void InspectorArchive::EndObject()
	{
		if (!Groups.empty()) {
			Groups.pop_back();
			ImGui::PopID();
		}
	}
}
