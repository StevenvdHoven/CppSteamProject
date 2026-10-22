#include "Editor/EntitiesWindow.h"
#include "Editor/EditorContext.h"
#include "Editor/InspectorArchive.h"
#include "Editor/SceneEditing.h"
#include "Core/World/GameObject.h"
#include "Core/World/Hierarchy.h"
#include "Core/World/World.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <string>

namespace Flex
{
	namespace
	{
		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		std::string ComponentList(const Archetype& archetype)
		{
			std::string text;
			for (ComponentId id : archetype.GetComponentIds()) {
				text += (text.empty() ? "" : ", ") + ComponentRegistery::GetInfo(id).Name;
			}
			return text.empty() ? std::string("(no components)") : text;
		}

		std::string EntityLabel(const World& world, Entity entity)
		{
			std::string label = "#" + std::to_string(entity.Index) + ":" + std::to_string(entity.Generation);
			if (const GameObjectInfo* info = world.GetComponent<GameObjectInfo>(entity)) {
				label += "  " + info->Name;
			}
			return label;
		}
	}

	EntitiesWindow::EntitiesWindow(EditorContext& context)
		: EditorWindow("Entities")
		, Context{ context }
	{
	}

	void EntitiesWindow::Draw()
	{
		if (!BeginWindow()) {
			EndWindow();
			return;
		}

		World& world = Context.EditorScene.GetWorld();
		size_t usedArchetypes = 0;
		for (const auto& archetype : world.GetArchetypes()) {
			usedArchetypes += archetype->GetCount() > 0 ? 1 : 0;
		}
		ImGui::Text("%u entities", world.GetEntityCount());
		ImGui::SameLine();
		ImGui::TextDisabled("| %d archetypes in use (%d total) | %u component types%s", static_cast<int>(usedArchetypes),
			static_cast<int>(world.GetArchetypes().size()), ComponentRegistery::GetCount(), Context.bIsPlaying ? " | playing" : "");
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##Filter", "Filter by component name", Filter, sizeof(Filter));

		if (ImGui::BeginTable("EntitiesLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableSetupColumn("Archetypes", ImGuiTableColumnFlags_WidthStretch, 0.55f);
			ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch, 0.45f);
			ImGui::TableNextColumn();
			if (ImGui::BeginChild("##Archetypes")) {
				DrawArchetypes();
			}
			ImGui::EndChild();
			ImGui::TableNextColumn();
			if (ImGui::BeginChild("##Details")) {
				DrawDetails();
			}
			ImGui::EndChild();
			ImGui::EndTable();
		}
		EndWindow();
	}

	void EntitiesWindow::DrawArchetypes()
	{
		World& world = Context.EditorScene.GetWorld();
		const std::string filter = ToLower(Filter);
		const auto& archetypes = world.GetArchetypes();
		for (size_t index = 0; index < archetypes.size(); ++index) {
			const Archetype& archetype = *archetypes[index];
			if (archetype.GetCount() == 0) {
				continue;
			}
			if (!filter.empty()) {
				bool bMatches = false;
				for (ComponentId id : archetype.GetComponentIds()) {
					bMatches = bMatches || ToLower(ComponentRegistery::GetInfo(id).Name).find(filter) != std::string::npos;
				}
				if (!bMatches) {
					continue;
				}
			}
			DrawArchetype(archetype, index);
		}
	}

	void EntitiesWindow::DrawArchetype(const Archetype& archetype, size_t index)
	{
		ImGui::PushID(static_cast<int>(index));
		const std::string label = ComponentList(archetype) + "  (" + std::to_string(archetype.GetCount()) + ")";
		if (ImGui::TreeNodeEx("##Archetype", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", label.c_str())) {
			// The rows of a big archetype (100k sprites) are clipped to what's on screen.
			const World& world = Context.EditorScene.GetWorld();
			const Entity* entities = archetype.GetEntities();
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(archetype.GetCount()));
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
					const Entity entity = entities[row];
					ImGui::PushID(row);
					if (ImGui::Selectable(EntityLabel(world, entity).c_str(), entity == SelectedEntity)) {
						SelectedEntity = entity;
					}
					ImGui::PopID();
				}
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	void EntitiesWindow::DrawDetails()
	{
		World& world = Context.EditorScene.GetWorld();
		if (!world.IsAlive(SelectedEntity)) {
			ImGui::TextDisabled("Select an entity on the left.");
			return;
		}
		const Entity entity = SelectedEntity;
		ImGui::TextUnformatted(EntityLabel(world, entity).c_str());
		if (world.HasComponent<GameObjectInfo>(entity)) {
			ImGui::SameLine();
			if (ImGui::SmallButton("Select in Hierarchy")) {
				Context.CurrentSelection.SelectEntity(entity);
			}
		} else {
			ImGui::TextDisabled("Lightweight entity (no GameObjectInfo): not saved, not in the Hierarchy.");
		}
		ImGui::Separator();

		// Copy: editing doesn't change the archetype, but keep the loop independent of the world.
		const auto componentSpan = world.GetComponentIds(entity);
		const std::vector<ComponentId> components(componentSpan.begin(), componentSpan.end());
		for (ComponentId id : components) {
			const ComponentTypeInfo& type = ComponentRegistery::GetInfo(id);
			ImGui::PushID(static_cast<int>(id));
			if (ImGui::CollapsingHeader(type.Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
				if (id == GetComponentId<Parent>()) {
					const Entity parent = Hierarchy::GetParent(world, entity);
					ImGui::Text("Parent: %s", parent.IsValid() ? EntityLabel(world, parent).c_str() : "(dead or none)");
				} else if (id == GetComponentId<Children>()) {
					ImGui::Text("%d children", static_cast<int>(Hierarchy::GetChildren(world, entity).size()));
				} else if (type.Serialize && ImGui::BeginTable("##Fields", 2, ImGuiTableFlags_SizingStretchProp)) {
					ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 90.0f);
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
					InspectorArchive archive(Context);
					type.Serialize(world.GetComponentById(entity, id), archive);
					ImGui::EndTable();
					if (archive.WasChanged()) {
						Context.MarkSceneChanged();
					}
					if (archive.WasEditFinished()) {
						SceneEditing::RecordUndo(Context, "Edit " + type.Name);
					}
				} else if (!type.Serialize) {
					ImGui::TextDisabled("%u bytes, no Serialize: not shown or saved", type.Size);
				}
			}
			ImGui::PopID();
		}
	}
}
