#include "Editor/HierarchyWindow.h"
#include "Editor/EditorContext.h"
#include "Editor/SceneEditing.h"
#include "Core/World/GameObject.h"
#include "Core/World/Hierarchy.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace Flex
{
	namespace
	{
		constexpr const char* RenamePopup = "Rename GameObject";
	}

	HierarchyWindow::HierarchyWindow(EditorContext& context)
		: EditorWindow("Hierarchy")
		, Context{ context }
	{
	}

	void HierarchyWindow::Draw()
	{
		if (!BeginWindow()) {
			EndWindow();
			return;
		}

		Scene& scene = Context.EditorScene;
		const Entity selection = Context.CurrentSelection.GetEntity();
		if (selection != RevealedSelection) {
			RevealedSelection = selection;
			NodesToOpen.clear();
			for (Entity parent = Hierarchy::GetParent(scene.GetWorld(), selection); parent.IsValid(); parent = Hierarchy::GetParent(scene.GetWorld(), parent)) {
				NodesToOpen.insert(parent);
			}
		}

		for (Entity root : scene.GetRoots()) {
			DrawNode(root);
		}

		// The rest of the window: drop target for "make root", click to deselect, right-click to create.
		const ImVec2 available = ImGui::GetContentRegionAvail();
		ImGui::InvisibleButton("##HierarchyEmpty", ImVec2(std::max(available.x, 1.0f), std::max(available.y, ImGui::GetFrameHeight() * 2.0f)));
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && Context.CurrentSelection.GetKind() == Selection::Kind::Entity) {
			Context.CurrentSelection.Clear();
		}
		AcceptDrop(Entity{});
		if (ImGui::BeginPopupContextItem("##HierarchyEmptyMenu")) {
			if (ImGui::MenuItem("Create Empty")) {
				PendingActions.push_back([this]() { SceneEditing::CreateEmpty(Context, Entity{}); });
			}
			ImGui::EndPopup();
		}

		// Keyboard shortcuts while the Hierarchy has focus.
		const Entity selected = Context.CurrentSelection.GetEntity();
		if (ImGui::IsWindowFocused() && selected.IsValid() && !ImGui::GetIO().WantTextInput) {
			if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
				PendingActions.push_back([this, selected]() { SceneEditing::Delete(Context, selected); });
			} else if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
				StartRename(selected);
			}
		}

		DrawRenamePopup();
		EndWindow();

		for (const auto& action : PendingActions) {
			action();
		}
		PendingActions.clear();
	}

	void HierarchyWindow::DrawNode(Entity entity)
	{
		World& world = Context.EditorScene.GetWorld();
		const GameObjectInfo* info = world.GetComponent<GameObjectInfo>(entity);
		if (!info) {
			return;
		}
		const auto children = Hierarchy::GetChildren(world, entity);

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (children.empty()) {
			flags |= ImGuiTreeNodeFlags_Leaf;
		}
		if (Context.CurrentSelection.IsEntitySelected(entity)) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		ImGui::PushID(static_cast<int>(entity.Index));
		if (NodesToOpen.erase(entity)) {
			ImGui::SetNextItemOpen(true);
		}
		const bool bOpen = ImGui::TreeNodeEx("##Node", flags, "%s", info->Name.empty() ? "(unnamed)" : info->Name.c_str());
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
			Context.CurrentSelection.SelectEntity(entity);
		}
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(EntityPayloadType, &entity, sizeof(Entity));
			ImGui::TextUnformatted(info->Name.c_str());
			ImGui::EndDragDropSource();
		}
		AcceptDrop(entity);
		DrawNodeContextMenu(entity);

		if (bOpen) {
			// Changes are queued (PendingActions), so this list stays valid while drawing.
			for (Entity child : children) {
				DrawNode(child);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	void HierarchyWindow::AcceptDrop(Entity target)
	{
		if (!ImGui::BeginDragDropTarget()) {
			return;
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EntityPayloadType)) {
			const Entity dragged = *static_cast<const Entity*>(payload->Data);
			if (dragged != target) {
				PendingActions.push_back([this, dragged, target]() { SceneEditing::Reparent(Context, dragged, target); });
			}
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadType)) {
			const Uid assetId = *static_cast<const Uid*>(payload->Data);
			PendingActions.push_back([this, assetId, target]() {
				// A child sits on its parent; a root at the world origin.
				const Vector2 position = target.IsValid() ? ComputeWorldMatrix(Context.EditorScene.GetWorld(), target).GetTranslation() : Vector2{};
				SceneEditing::CreateFromAsset(Context, assetId, target, position);
			});
		}
		ImGui::EndDragDropTarget();
	}

	void HierarchyWindow::DrawNodeContextMenu(Entity entity)
	{
		if (!ImGui::BeginPopupContextItem("##NodeMenu")) {
			return;
		}
		Context.CurrentSelection.SelectEntity(entity);
		if (ImGui::MenuItem("Create Empty Child")) {
			PendingActions.push_back([this, entity]() {
				SceneEditing::CreateEmpty(Context, entity, ComputeWorldMatrix(Context.EditorScene.GetWorld(), entity).GetTranslation());
			});
		}
		if (ImGui::MenuItem("Rename", "F2")) {
			StartRename(entity);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Move Up")) {
			PendingActions.push_back([this, entity]() { SceneEditing::MoveSibling(Context, entity, -1); });
		}
		if (ImGui::MenuItem("Move Down")) {
			PendingActions.push_back([this, entity]() { SceneEditing::MoveSibling(Context, entity, 1); });
		}
		if (ImGui::MenuItem("Unparent", nullptr, false, Hierarchy::GetParent(Context.EditorScene.GetWorld(), entity).IsValid())) {
			PendingActions.push_back([this, entity]() { SceneEditing::Reparent(Context, entity, Entity{}); });
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Delete", "Del")) {
			PendingActions.push_back([this, entity]() { SceneEditing::Delete(Context, entity); });
		}
		ImGui::EndPopup();
	}

	void HierarchyWindow::StartRename(Entity entity)
	{
		const GameObjectInfo* info = Context.EditorScene.GetWorld().GetComponent<GameObjectInfo>(entity);
		if (!info) {
			return;
		}
		RenameTarget = entity;
		std::snprintf(RenameBuffer, sizeof(RenameBuffer), "%s", info->Name.c_str());
		bOpenRenamePopup = true;
	}

	void HierarchyWindow::DrawRenamePopup()
	{
		if (bOpenRenamePopup) {
			ImGui::OpenPopup(RenamePopup);
			bOpenRenamePopup = false;
		}
		if (!ImGui::BeginPopupModal(RenamePopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			return;
		}
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}
		const bool bEnter = ImGui::InputText("##Name", RenameBuffer, sizeof(RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
		if (bEnter || ImGui::Button("OK")) {
			const Entity target = RenameTarget;
			const std::string name = RenameBuffer;
			PendingActions.push_back([this, target, name]() { SceneEditing::Rename(Context, target, name); });
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
