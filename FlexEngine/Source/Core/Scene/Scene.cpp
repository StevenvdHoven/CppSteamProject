#include "Core/Scene/Scene.h"
#include "Core/World/GameObject.h"
#include "Core/World/Hierarchy.h"
#include <algorithm>
#include <unordered_set>

namespace Flex
{
	FLEX_REGISTER_COMPONENT(GameObjectInfo)

	Scene::Scene()
		: SceneWorld{ std::make_unique<World>() }
	{
	}

	Scene::~Scene() = default;

	Entity Scene::CreateGameObject(const std::string& name, Entity parent, const Transform& local, Uid id)
	{
		if (!id.IsValid() || EntitiesByUid.contains(id)) {
			id = Uid::Generate();
		}
		const Entity entity = SceneWorld->Spawn(local, WorldTransform{ local.ToMatrix() }, GameObjectInfo{ name, id });
		EntitiesByUid[id] = entity;
		if (parent.IsValid() && SceneWorld->IsAlive(parent)) {
			Hierarchy::SetParent(*SceneWorld, entity, parent, false);
		} else {
			Roots.push_back(entity);
		}
		return entity;
	}

	void Scene::DestroyGameObject(Entity entity)
	{
		if (!SceneWorld->IsAlive(entity)) {
			return;
		}
		// Forget the Uids of the whole subtree first.
		std::vector<Entity> subtree{ entity };
		for (size_t i = 0; i < subtree.size(); ++i) {
			for (Entity child : Hierarchy::GetChildren(*SceneWorld, subtree[i])) {
				subtree.push_back(child);
			}
		}
		for (Entity doomed : subtree) {
			if (const GameObjectInfo* info = SceneWorld->GetComponent<GameObjectInfo>(doomed)) {
				EntitiesByUid.erase(info->Id);
			}
		}
		std::erase(Roots, entity);
		Hierarchy::DestroyWithChildren(*SceneWorld, entity);
	}

	bool Scene::SetParent(Entity child, Entity parent, int siblingIndex)
	{
		if (!SceneWorld->IsAlive(child)) {
			return false;
		}
		if (!Hierarchy::SetParent(*SceneWorld, child, parent, true)) {
			return false;
		}
		std::erase(Roots, child);
		if (!parent.IsValid()) {
			Roots.push_back(child);
		}
		if (siblingIndex >= 0) {
			SetSiblingIndex(child, siblingIndex);
		}
		return true;
	}

	int Scene::GetSiblingIndex(Entity entity)
	{
		const Entity parent = Hierarchy::GetParent(*SceneWorld, entity);
		if (parent.IsValid()) {
			const auto children = Hierarchy::GetChildren(*SceneWorld, parent);
			const auto it = std::find(children.begin(), children.end(), entity);
			return it != children.end() ? static_cast<int>(it - children.begin()) : -1;
		}
		const std::vector<Entity>& roots = GetRoots();
		const auto it = std::find(roots.begin(), roots.end(), entity);
		return it != roots.end() ? static_cast<int>(it - roots.begin()) : -1;
	}

	void Scene::SetSiblingIndex(Entity entity, int index)
	{
		const Entity parent = Hierarchy::GetParent(*SceneWorld, entity);
		if (parent.IsValid()) {
			Hierarchy::SetSiblingIndex(*SceneWorld, entity, index);
			return;
		}
		SyncRoots();
		const auto it = std::find(Roots.begin(), Roots.end(), entity);
		if (it == Roots.end()) {
			return;
		}
		Roots.erase(it);
		const size_t position = std::min(static_cast<size_t>(std::max(index, 0)), Roots.size());
		Roots.insert(Roots.begin() + static_cast<std::ptrdiff_t>(position), entity);
	}

	const std::vector<Entity>& Scene::GetRoots()
	{
		SyncRoots();
		return Roots;
	}

	void Scene::SyncRoots()
	{
		// Keep the known order; drop dead or parented entries; append roots we didn't know about.
		std::erase_if(Roots, [this](Entity entity) {
			return !SceneWorld->IsAlive(entity) || Hierarchy::GetParent(*SceneWorld, entity).IsValid();
		});
		if (Roots.size() == EntitiesByUid.size()) {
			return; // every GameObject is a known root
		}
		const std::unordered_set<Entity> known(Roots.begin(), Roots.end());
		SceneWorld->Each<const GameObjectInfo>([&](Entity entity, const GameObjectInfo&) {
			if (!known.contains(entity) && !Hierarchy::GetParent(*SceneWorld, entity).IsValid()) {
				Roots.push_back(entity);
			}
		});
	}

	Entity Scene::FindByUid(Uid id) const
	{
		const auto it = EntitiesByUid.find(id);
		return it != EntitiesByUid.end() && SceneWorld->IsAlive(it->second) ? it->second : Entity{};
	}

	bool Scene::IsGameObject(Entity entity) const
	{
		return SceneWorld->HasComponent<GameObjectInfo>(entity);
	}

	void Scene::Clear()
	{
		SceneWorld = std::make_unique<World>();
		Roots.clear();
		EntitiesByUid.clear();
	}
}
