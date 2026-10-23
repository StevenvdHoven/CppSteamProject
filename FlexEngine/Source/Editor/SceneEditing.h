#pragma once
#include "Core/Math/Vector2.h"
#include "Core/Uid.h"
#include "Core/World/ComponentRegistery.h"
#include "Core/World/Entity.h"
#include <string>

namespace Flex
{
	struct EditorContext;

	// Every change the editor windows make to the edited scene goes through here, so the
	// Hierarchy, Viewport and Inspector behave the same (and undo can hook in one place).
	// Functions that create something select it.
	namespace SceneEditing
	{
		// True for asset types that can become a GameObject by dragging (textures, sprites).
		bool CanCreateFromAsset(const EditorContext& context, Uid assetId);

		// A GameObject with a SpriteRenderer showing the asset, placed at worldPosition under
		// parent (Entity{} = root). Entity{} for asset types that can't be placed.
		Entity CreateFromAsset(EditorContext& context, Uid assetId, Entity parent, const Vector2& worldPosition);

		Entity CreateEmpty(EditorContext& context, Entity parent, const Vector2& worldPosition = {});

		// Deletes the GameObject and its children.
		void Delete(EditorContext& context, Entity entity);

		// Copies the GameObject and its children (new Uids) right after it, and selects the copy.
		Entity Duplicate(EditorContext& context, Entity entity);

		// Keeps the world position. siblingIndex -1 = last. False for a cycle or dead entities.
		bool Reparent(EditorContext& context, Entity child, Entity parent, int siblingIndex = -1);

		// Moves up (-1) or down (+1) among its siblings.
		void MoveSibling(EditorContext& context, Entity entity, int delta);

		// bRecordUndo false: for typing, where the undo step is recorded when the edit ends.
		void Rename(EditorContext& context, Entity entity, const std::string& name, bool bRecordUndo = true);

		// Sets the local Transform so the entity ends up at worldPosition. Called every frame
		// while dragging, so it doesn't record undo: call RecordUndo when the drag ends.
		void SetWorldPosition(EditorContext& context, Entity entity, const Vector2& worldPosition);

		// Adds a default component of the type (does nothing if the entity already has it).
		void AddComponent(EditorContext& context, Entity entity, ComponentId type);
		// Transform and GameObjectInfo can't be removed.
		void RemoveComponent(EditorContext& context, Entity entity, ComponentId type);
		bool CanRemoveComponent(ComponentId type);

		// Records the current scene as an undo step named after the edit.
		void RecordUndo(EditorContext& context, const std::string& name);

		// Converts a world position into the local space of parent (Entity{} = world).
		Vector2 WorldToLocal(EditorContext& context, Entity parent, const Vector2& worldPosition);
	}
}
