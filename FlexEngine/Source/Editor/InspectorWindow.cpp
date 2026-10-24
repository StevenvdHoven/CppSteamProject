#include "Editor/InspectorWindow.h"
#include "Editor/EditorContext.h"
#include "Editor/InspectorArchive.h"
#include "Editor/SceneEditing.h"
#include "Editor/TextureImporter.h"
#include "Core/Assets/AssetManager.h"
#include "Core/Assets/Sprite.h"
#include "Core/FileSystem.h"
#include "Core/Scene/SceneSerializer.h"
#include "Core/World/GameObject.h"
#include "Core/World/World.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <unordered_map>

namespace Flex
{
	InspectorWindow::InspectorWindow(EditorContext& context)
		: EditorWindow("Inspector")
		, Context{ context }
	{
	}

	void InspectorWindow::Draw()
	{
		if (!BeginWindow()) {
			EndWindow();
			return;
		}

		switch (Context.CurrentSelection.GetKind()) {
		case Selection::Kind::Asset:
			DrawAsset(Context.CurrentSelection.GetAsset());
			break;
		case Selection::Kind::Entity:
			DrawEntity(Context.CurrentSelection.GetEntity());
			break;
		case Selection::Kind::None:
			ImGui::TextDisabled("Nothing selected.");
			break;
		}

		EndWindow();
	}

	void InspectorWindow::DrawAsset(Uid assetId)
	{
		const AssetInfo* asset = Context.Assets.Find(assetId);
		if (!asset) {
			ImGui::TextDisabled("The selected asset no longer exists.");
			return;
		}

		ImGui::TextUnformatted(asset->Name.c_str());
		ImGui::TextDisabled("%s", asset->Type.c_str());
		ImGui::Separator();

		if (ImGui::BeginTable("AssetInfo", 2, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 70.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("Path");
			ImGui::TableNextColumn();
			ImGui::TextWrapped("Content/%s", PathToGenericUtf8(asset->Path).c_str());
			ImGui::TableNextColumn();
			ImGui::TextDisabled("Id");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(asset->Id.ToString().c_str());
			if (const AssetInfo* parent = asset->IsSubAsset() ? Context.Assets.Find(asset->ParentId) : nullptr) {
				ImGui::TableNextColumn();
				ImGui::TextDisabled("Part of");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(parent->Name.c_str());
			}
			ImGui::EndTable();
		}

		// Preview, as large as the window allows (square area).
		const float previewSize = std::min(ImGui::GetContentRegionAvail().x, 256.0f);
		const ImVec2 previewMin = ImGui::GetCursorScreenPos();
		const ImVec2 previewMax{ previewMin.x + previewSize, previewMin.y + previewSize };
		if (Context.Thumbnails.Draw(ImGui::GetWindowDrawList(), assetId, previewMin, previewMax)) {
			ImGui::Dummy(ImVec2(previewSize, previewSize));
		}

		if (asset->Type == TextureImporter::TypeName) {
			DrawTextureSettings(*asset);
		} else if (asset->Type == TextureImporter::SpriteTypeName) {
			if (auto sprite = std::dynamic_pointer_cast<Sprite>(AssetManager::FindLoaded(assetId))) {
				const SpriteRect& rect = sprite->GetRect();
				ImGui::Text("Rect: %d, %d  %d x %d", rect.X, rect.Y, rect.Width, rect.Height);
				ImGui::Text("Pivot: %.2f, %.2f", sprite->GetPivotX(), sprite->GetPivotY());
			}
		}

		ImGui::Spacing();
		if (Context.OpenAsset && ImGui::Button("Open Editor")) {
			Context.OpenAsset(assetId);
		}
	}

	void InspectorWindow::DrawEntity(Entity entity)
	{
		World& world = Context.EditorScene.GetWorld();
		GameObjectInfo* info = world.GetComponent<GameObjectInfo>(entity);
		if (!info) {
			ImGui::TextDisabled("The selected GameObject no longer exists.");
			return;
		}

		if (NameEntity != entity || (!ImGui::IsAnyItemActive() && info->Name != NameBuffer)) {
			NameEntity = entity;
			std::snprintf(NameBuffer, sizeof(NameBuffer), "%s", info->Name.c_str());
		}
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputText("##Name", NameBuffer, sizeof(NameBuffer))) {
			SceneEditing::Rename(Context, entity, NameBuffer, false);
		}
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			SceneEditing::RecordUndo(Context, std::string("Rename ") + NameBuffer);
		}
		ImGui::TextDisabled("GameObject %s", info->Id.ToString().c_str());
		ImGui::Separator();

		DrawComponents(entity);
		DrawAddComponent(entity);

		if (PendingAction) {
			PendingAction();
			PendingAction = nullptr;
		}
	}

	namespace
	{
		std::unordered_map<std::string, InspectorWindow::ComponentDrawer>& ComponentDrawers()
		{
			static std::unordered_map<std::string, InspectorWindow::ComponentDrawer> drawers;
			return drawers;
		}
	}

	void InspectorWindow::RegisterComponentDrawer(const std::string& componentName, ComponentDrawer drawer)
	{
		ComponentDrawers()[componentName] = drawer;
	}

	void InspectorWindow::DrawComponents(Entity entity)
	{
		World& world = Context.EditorScene.GetWorld();
		for (const ComponentTypeInfo* type : SceneSerializer::GetSavedComponents(world, entity)) {
			ImGui::PushID(type->Name.c_str());
			const bool bOpen = ImGui::CollapsingHeader(type->Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

			// Right-click on the header, or the small button on its right: remove.
			const bool bRemovable = SceneEditing::CanRemoveComponent(type->Id);
			if (ImGui::BeginPopupContextItem("##ComponentMenu")) {
				if (ImGui::MenuItem("Remove Component", nullptr, false, bRemovable)) {
					const ComponentId id = type->Id;
					PendingAction = [this, entity, id]() { SceneEditing::RemoveComponent(Context, entity, id); };
				}
				ImGui::EndPopup();
			}
			if (bRemovable) {
				ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
				if (ImGui::SmallButton("x")) {
					const ComponentId id = type->Id;
					PendingAction = [this, entity, id]() { SceneEditing::RemoveComponent(Context, entity, id); };
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Remove %s", type->Name.c_str());
				}
			}

			if (bOpen && ImGui::BeginTable("##Fields", 2, ImGuiTableFlags_SizingStretchProp)) {
				ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
				InspectorArchive archive(Context);
				void* component = world.GetComponentById(entity, type->Id);
				if (const auto drawer = ComponentDrawers().find(type->Name); drawer != ComponentDrawers().end()) {
					drawer->second(archive, component);
				} else {
					type->Serialize(component, archive);
				}
				ImGui::EndTable();

				if (archive.WasChanged()) {
					Context.MarkSceneChanged();
				}
				// One undo step per finished edit (a whole drag, a typed value, a dropped asset).
				if (archive.WasEditFinished()) {
					SceneEditing::RecordUndo(Context, "Edit " + type->Name);
				}
			}
			ImGui::PopID();
		}

		if (const PreservedComponents* preserved = world.GetComponent<PreservedComponents>(entity)) {
			ImGui::Spacing();
			ImGui::TextDisabled("%d unknown component(s) from the scene file are kept as they are.", static_cast<int>(preserved->Sections.size()));
		}
	}

	void InspectorWindow::DrawAddComponent(Entity entity)
	{
		ImGui::Spacing();
		ImGui::Separator();
		const float width = std::min(ImGui::GetContentRegionAvail().x, 240.0f);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - width) * 0.5f);
		if (ImGui::Button("Add Component", ImVec2(width, 0.0f))) {
			ComponentFilter[0] = '\0';
			ImGui::OpenPopup("##AddComponent");
		}
		if (!ImGui::BeginPopup("##AddComponent")) {
			return;
		}

		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::InputTextWithHint("##Filter", "Search", ComponentFilter, sizeof(ComponentFilter));
		std::string filter = ComponentFilter;
		std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		World& world = Context.EditorScene.GetWorld();
		int shown = 0;
		// Every registered type that can be created and saved, and that the entity doesn't have yet.
		for (const ComponentTypeInfo* type : ComponentRegistery::GetAll()) {
			if (!type->Serialize || !type->DefaultConstruct || type->Id == GetComponentId<GameObjectInfo>() || world.GetComponentById(entity, type->Id)) {
				continue;
			}
			std::string name = type->Name;
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (!filter.empty() && name.find(filter) == std::string::npos) {
				continue;
			}
			++shown;
			if (ImGui::Selectable(type->Name.c_str())) {
				const ComponentId id = type->Id;
				PendingAction = [this, entity, id]() { SceneEditing::AddComponent(Context, entity, id); };
				ImGui::CloseCurrentPopup();
			}
		}
		if (shown == 0) {
			ImGui::TextDisabled("No components to add.");
		}
		ImGui::EndPopup();
	}

	void InspectorWindow::DrawTextureSettings(const AssetInfo& asset)
	{
		if (SettingsAssetId != asset.Id || SettingsRevision != Context.AssetRevision) {
			SettingsAssetId = asset.Id;
			SettingsRevision = Context.AssetRevision;
			Settings = TextureImportSettings{};
			SettingsError.clear();
			TextureImporter::ReadSettings(Context.Assets, asset, Settings, SettingsError);
		}

		ImGui::SeparatorText("Import Settings");
		if (!SettingsError.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", SettingsError.c_str());
			return;
		}

		constexpr TextureFilter Filters[] = { TextureFilter::PixelArt, TextureFilter::Nearest, TextureFilter::Linear };
		if (ImGui::BeginCombo("Filter", ToString(Settings.Filter))) {
			for (TextureFilter filter : Filters) {
				if (ImGui::Selectable(ToString(filter), filter == Settings.Filter) && filter != Settings.Filter) {
					Settings.Filter = filter;
					if (!TextureImporter::WriteSettings(Context.Assets, asset, Settings, SettingsError)) {
						SettingsRevision = UINT32_MAX; // re-read next frame
					} else if (auto texture = std::dynamic_pointer_cast<Texture>(AssetManager::FindLoaded(asset.Id))) {
						texture->SetFilter(filter); // every user of this texture sees it right away
					}
				}
			}
			ImGui::EndCombo();
		}
	}
}
