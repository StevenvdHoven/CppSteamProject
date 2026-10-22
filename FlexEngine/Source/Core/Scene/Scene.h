#pragma once
#include "Core/Core.h"
#include "Core/Math/Vector2.h"
#include "Core/Uid.h"
#include "Core/World/Entity.h"
#include "Core/World/Transform.h"
#include "Core/World/World.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Flex
{
	// A World of GameObjects plus what a world doesn't know: the order of the root objects
	// (children are ordered by their Children list) and a Uid -> Entity lookup.
	// The editor edits one Scene; scene files (later) load into one.
	class FLEX_API Scene
	{
	public:
		Scene();
		~Scene();

		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;

		World& GetWorld() { return *SceneWorld; }
		const World& GetWorld() const { return *SceneWorld; }

		// Spawns Transform + WorldTransform + GameObjectInfo. local is relative to parent
		// (Entity{} = a new root, added at the end). An invalid id gets a new random one.
		Entity CreateGameObject(const std::string& name, Entity parent = {}, const Transform& local = {}, Uid id = {});

		// Destroys the GameObject and everything below it.
		void DestroyGameObject(Entity entity);

		// Moves child under parent (Entity{} = root) at siblingIndex (-1 = last), keeping its
		// world position. Returns false for dead entities or a cycle.
		bool SetParent(Entity child, Entity parent, int siblingIndex = -1);

		// Position among its siblings (roots or children of the same parent).
		int GetSiblingIndex(Entity entity);
		void SetSiblingIndex(Entity entity, int index);

		// Root GameObjects in order. Also picks up changes made directly through Hierarchy.
		const std::vector<Entity>& GetRoots();

		Entity FindByUid(Uid id) const;
		bool IsGameObject(Entity entity) const;
		size_t GetGameObjectCount() const { return EntitiesByUid.size(); }

		// Removes every GameObject (and anything else in the world).
		void Clear();

	private:
		void SyncRoots();

	private:
		std::unique_ptr<World> SceneWorld;
		std::vector<Entity> Roots;
		std::unordered_map<Uid, Entity> EntitiesByUid;
	};
}
