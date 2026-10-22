#include "Editor/SceneEditing.h"
#include "Editor/EditorContext.h"
#include "Editor/TextureImporter.h"
#include "Core/Graphics/SpriteRenderer.h"
#include "Core/World/GameObject.h"
#include "Core/World/Hierarchy.h"
#include "Core/World/Transform.h"
#include "Core/World/World.h"

namespace Flex
{
	bool SceneEditing::CanCreateFromAsset(const EditorContext& context, Uid assetId)
	{
		const AssetInfo* asset = context.Assets.Find(assetId);
		return asset && (asset->Type == TextureImporter::TypeName || asset->Type == TextureImporter::SpriteTypeName);
	}

	Vector2 SceneEditing::WorldToLocal(EditorContext& context, Entity parent, const Vector2& worldPosition)
	{
		if (!parent.IsValid()) {
			return worldPosition;
		}
		return ComputeWorldMatrix(context.EditorScene.GetWorld(), parent).Inverse().TransformPoint(worldPosition);
	}

	Entity SceneEditing::CreateFromAsset(EditorContext& context, Uid assetId, Entity parent, const Vector2& worldPosition)
	{
		if (!CanCreateFromAsset(context, assetId)) {
			return {};
		}
		const AssetInfo* asset = context.Assets.Find(assetId);
		Transform local;
		local.Position = WorldToLocal(context, parent, worldPosition);
		const Entity entity = context.EditorScene.CreateGameObject(asset->Name, parent, local);
		SpriteRenderer renderer;
		renderer.AssetId = assetId;
		context.EditorScene.GetWorld().AddComponent(entity, renderer);
		context.CurrentSelection.SelectEntity(entity);
		context.MarkSceneChanged();
		RecordUndo(context, "Create " + asset->Name);
		return entity;
	}

	Entity SceneEditing::CreateEmpty(EditorContext& context, Entity parent, const Vector2& worldPosition)
	{
		Transform local;
		local.Position = WorldToLocal(context, parent, worldPosition);
		const Entity entity = context.EditorScene.CreateGameObject("GameObject", parent, local);
		context.CurrentSelection.SelectEntity(entity);
		context.MarkSceneChanged();
		RecordUndo(context, "Create GameObject");
		return entity;
	}

	void SceneEditing::Delete(EditorContext& context, Entity entity)
	{
		if (context.CurrentSelection.GetKind() == Selection::Kind::Entity) {
			// Deleting the selection or one of its ancestors clears it.
			for (Entity e = context.CurrentSelection.GetEntity(); e.IsValid(); e = Hierarchy::GetParent(context.EditorScene.GetWorld(), e)) {
				if (e == entity) {
					context.CurrentSelection.Clear();
					break;
				}
			}
		}
		const GameObjectInfo* info = context.EditorScene.GetWorld().GetComponent<GameObjectInfo>(entity);
		const std::string name = info ? info->Name : std::string("GameObject");
		context.EditorScene.DestroyGameObject(entity);
		context.MarkSceneChanged();
		RecordUndo(context, "Delete " + name);
	}

	bool SceneEditing::Reparent(EditorContext& context, Entity child, Entity parent, int siblingIndex)
	{
		if (!context.EditorScene.SetParent(child, parent, siblingIndex)) {
			return false;
		}
		context.MarkSceneChanged();
		RecordUndo(context, "Reparent");
		return true;
	}

	void SceneEditing::MoveSibling(EditorContext& context, Entity entity, int delta)
	{
		const int index = context.EditorScene.GetSiblingIndex(entity);
		if (index >= 0 && index + delta >= 0) {
			context.EditorScene.SetSiblingIndex(entity, index + delta);
			context.MarkSceneChanged();
			RecordUndo(context, delta < 0 ? "Move Up" : "Move Down");
		}
	}

	void SceneEditing::Rename(EditorContext& context, Entity entity, const std::string& name, bool bRecordUndo)
	{
		if (GameObjectInfo* info = context.EditorScene.GetWorld().GetComponent<GameObjectInfo>(entity)) {
			info->Name = name;
			context.MarkSceneChanged();
			if (bRecordUndo) {
				RecordUndo(context, "Rename " + name);
			}
		}
	}

	void SceneEditing::SetWorldPosition(EditorContext& context, Entity entity, const Vector2& worldPosition)
	{
		World& world = context.EditorScene.GetWorld();
		if (Transform* transform = world.GetComponent<Transform>(entity)) {
			transform->Position = WorldToLocal(context, Hierarchy::GetParent(world, entity), worldPosition);
			context.MarkSceneChanged();
		}
	}

	void SceneEditing::AddComponent(EditorContext& context, Entity entity, ComponentId type)
	{
		World& world = context.EditorScene.GetWorld();
		if (world.GetComponentById(entity, type) || !world.AddComponentById(entity, type)) {
			return;
		}
		context.MarkSceneChanged();
		RecordUndo(context, "Add " + ComponentRegistery::GetInfo(type).Name);
	}

	bool SceneEditing::CanRemoveComponent(ComponentId type)
	{
		return type != GetComponentId<Transform>() && type != GetComponentId<GameObjectInfo>();
	}

	void SceneEditing::RemoveComponent(EditorContext& context, Entity entity, ComponentId type)
	{
		if (!CanRemoveComponent(type) || !context.EditorScene.GetWorld().RemoveComponentById(entity, type)) {
			return;
		}
		context.MarkSceneChanged();
		RecordUndo(context, "Remove " + ComponentRegistery::GetInfo(type).Name);
	}

	void SceneEditing::RecordUndo(EditorContext& context, const std::string& name)
	{
		context.Undo.Record(context, name);
	}
}
