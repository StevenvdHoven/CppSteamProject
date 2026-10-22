#pragma once
#include "Core/Core.h"
#include "Core/World/ComponentRegistery.h"
#include "Core/World/Entity.h"
#include <span>
#include <vector>

namespace Flex
{
	class World;

	// Parent/child links between entities. Any entity can be parented (GameObjects and
	// lightweight entities alike). Both sides are kept in sync by Hierarchy::SetParent,
	// so don't edit these components directly.
	struct Parent
	{
		FLEX_COMPONENT(Parent)
		Entity Value;
	};

	// Removed again when the last child leaves. Can briefly contain dead entities after a
	// plain World::Destroy; UpdateWorldTransforms cleans them up.
	struct Children
	{
		FLEX_COMPONENT(Children)
		std::vector<Entity> Entities;
	};

	namespace Hierarchy
	{
		// Makes parent the parent of child; Entity{} makes child a root again.
		// bKeepWorldTransform: the child stays where it is in the world (its local Transform is
		// recalculated); false keeps the local values, so the child moves with its new parent.
		// Returns false for a dead child or parent, or when it would create a cycle.
		// Structural change: inside World::Each, use world.GetCommands().Defer(...).
		// The child's WorldTransform updates at the next UpdateWorldTransforms.
		FLEX_API bool SetParent(World& world, Entity child, Entity parent, bool bKeepWorldTransform = true);

		// Entity{} for a root or a dead entity.
		FLEX_API Entity GetParent(const World& world, Entity entity);

		// Direct children; empty when there are none. Valid until the next structural change.
		FLEX_API std::span<const Entity> GetChildren(const World& world, Entity entity);

		// Moves the entity to position index among its parent's children (clamped). Not for roots.
		FLEX_API void SetSiblingIndex(World& world, Entity entity, int index);

		// The top-most parent (the entity itself for a root).
		FLEX_API Entity GetRoot(const World& world, Entity entity);

		// Destroys the entity and everything below it, and removes it from its parent's children.
		// (World::Destroy only destroys the one entity and leaves its children orphaned.)
		FLEX_API void DestroyWithChildren(World& world, Entity entity);
	}
}
