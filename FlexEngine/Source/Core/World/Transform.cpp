#include "Core/World/Transform.h"
#include "Core/World/Hierarchy.h"
#include "Core/World/World.h"
#include <SDL3/SDL.h>
#include <algorithm>

namespace Flex
{
	namespace
	{
		struct TransformColumns
		{
			Transform* Locals = nullptr;
			WorldTransform* Worlds = nullptr;
		};

		bool GetTransformColumns(const Archetype& archetype, TransformColumns& outColumns)
		{
			const int32_t local = archetype.FindColumn(GetComponentId<Transform>());
			const int32_t world = archetype.FindColumn(GetComponentId<WorldTransform>());
			if (local < 0 || world < 0) {
				return false;
			}
			outColumns.Locals = static_cast<Transform*>(archetype.GetColumnData(local));
			outColumns.Worlds = static_cast<WorldTransform*>(archetype.GetColumnData(world));
			return true;
		}

		Matrix2x3 GetLocalMatrix(const World& world, Entity entity)
		{
			const Transform* transform = world.GetComponent<Transform>(entity);
			return transform ? transform->ToMatrix() : Matrix2x3::Identity();
		}

		// parentWorld * local for every living child, then their children. Drops dead children.
		void UpdateChildren(World& world, Entity parent, const Matrix2x3& parentWorld)
		{
			Children* children = world.GetComponent<Children>(parent);
			if (!children) {
				return;
			}
			std::erase_if(children->Entities, [&](Entity child) { return !world.IsAlive(child); });
			// Nothing below makes structural changes, so this list and pointer stay valid.
			for (Entity child : children->Entities) {
				const Matrix2x3 childWorld = parentWorld * GetLocalMatrix(world, child);
				if (WorldTransform* worldTransform = world.GetComponent<WorldTransform>(child)) {
					worldTransform->Matrix = childWorld;
				}
				UpdateChildren(world, child, childWorld);
			}
		}
	}

	void UpdateWorldTransforms(World& world)
	{
		const ComponentId parentId = GetComponentId<Parent>();
		const ComponentId childrenId = GetComponentId<Children>();
#if FLEX_DEBUG_TOOLS
		static bool bWarnedMissingWorld = false;
		static bool bWarnedOrphan = false;
#endif

		// 1. Roots: world = local. One flat loop per archetype.
		for (const std::unique_ptr<Archetype>& archetype : world.GetArchetypes()) {
			if (archetype->GetCount() == 0 || archetype->FindColumn(parentId) >= 0) {
				continue;
			}
			TransformColumns columns;
			if (!GetTransformColumns(*archetype, columns)) {
#if FLEX_DEBUG_TOOLS
				if (!bWarnedMissingWorld && archetype->FindColumn(GetComponentId<Transform>()) >= 0) {
					SDL_Log("UpdateWorldTransforms: entities with Transform but no WorldTransform are skipped. Spawn both.");
					bWarnedMissingWorld = true;
				}
#endif
				continue;
			}
			const uint32_t count = archetype->GetCount();
			for (uint32_t row = 0; row < count; ++row) {
				columns.Worlds[row].Matrix = columns.Locals[row].ToMatrix();
			}
		}

		// 2. Children, depth-first from every root that has children.
		//    Collected first, because the walk touches components through World.
		std::vector<Entity> roots;
		std::vector<Entity> orphans;
		for (const std::unique_ptr<Archetype>& archetype : world.GetArchetypes()) {
			if (archetype->GetCount() == 0) {
				continue;
			}
			const bool bHasParent = archetype->FindColumn(parentId) >= 0;
			const bool bHasChildren = archetype->FindColumn(childrenId) >= 0;
			const Entity* entities = archetype->GetEntities();
			if (!bHasParent && bHasChildren) {
				roots.insert(roots.end(), entities, entities + archetype->GetCount());
			} else if (bHasParent) {
				// A child whose parent was destroyed with World::Destroy is never reached from a root.
				const Parent* links = static_cast<const Parent*>(archetype->GetColumnData(archetype->FindColumn(parentId)));
				for (uint32_t row = 0; row < archetype->GetCount(); ++row) {
					if (!world.IsAlive(links[row].Value)) {
						orphans.push_back(entities[row]);
					}
				}
			}
		}

		for (Entity root : roots) {
			UpdateChildren(world, root, GetLocalMatrix(world, root));
		}

		// 3. Orphans behave like roots until they're re-parented or destroyed.
		for (Entity orphan : orphans) {
#if FLEX_DEBUG_TOOLS
			if (!bWarnedOrphan) {
				SDL_Log("UpdateWorldTransforms: entity %u has a destroyed parent and is treated as a root. Use Hierarchy::DestroyWithChildren.", orphan.Index);
				bWarnedOrphan = true;
			}
#endif
			const Matrix2x3 orphanWorld = GetLocalMatrix(world, orphan);
			if (WorldTransform* worldTransform = world.GetComponent<WorldTransform>(orphan)) {
				worldTransform->Matrix = orphanWorld;
			}
			UpdateChildren(world, orphan, orphanWorld);
		}
	}

	Matrix2x3 ComputeWorldMatrix(const World& world, Entity entity)
	{
		if (!world.IsAlive(entity)) {
			return Matrix2x3::Identity();
		}
		Matrix2x3 result = GetLocalMatrix(world, entity);
		for (Entity parent = Hierarchy::GetParent(world, entity); parent.IsValid(); parent = Hierarchy::GetParent(world, parent)) {
			result = GetLocalMatrix(world, parent) * result;
		}
		return result;
	}
}
