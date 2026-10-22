#include "Core/World/Hierarchy.h"
#include "Core/World/Transform.h"
#include "Core/World/World.h"
#include <algorithm>

namespace Flex
{
	FLEX_REGISTER_COMPONENT(Parent)
	FLEX_REGISTER_COMPONENT(Children)

	namespace
	{
		void RemoveFromChildren(World& world, Entity parent, Entity child)
		{
			Children* children = world.GetComponent<Children>(parent);
			if (!children) {
				return;
			}
			std::erase(children->Entities, child);
			if (children->Entities.empty()) {
				world.RemoveComponent<Children>(parent);
			}
		}
	}

	bool Hierarchy::SetParent(World& world, Entity child, Entity parent, bool bKeepWorldTransform)
	{
		if (!world.IsAlive(child) || child == parent) {
			return false;
		}
		if (parent.IsValid()) {
			if (!world.IsAlive(parent)) {
				return false;
			}
			// Refuse cycles: child may not be parent or one of parent's ancestors.
			for (Entity ancestor = parent; ancestor.IsValid(); ancestor = GetParent(world, ancestor)) {
				if (ancestor == child) {
					return false;
				}
			}
		}

		const Entity oldParent = GetParent(world, child);
		const Parent* currentLink = world.GetComponent<Parent>(child);
		if (oldParent == parent && (parent.IsValid() || !currentLink)) {
			return true;
		}

		const Matrix2x3 childWorld = bKeepWorldTransform ? ComputeWorldMatrix(world, child) : Matrix2x3{};

		// Detach (also from a parent that is already dead: its link is just dropped).
		if (currentLink && world.IsAlive(currentLink->Value)) {
			RemoveFromChildren(world, currentLink->Value, child);
		}

		if (parent.IsValid()) {
			world.AddComponent<Parent>(child, Parent{ parent });
			Children* children = world.GetComponent<Children>(parent);
			if (!children) {
				children = world.AddComponent<Children>(parent);
			}
			children->Entities.push_back(child);
		} else {
			world.RemoveComponent<Parent>(child);
		}

		if (bKeepWorldTransform) {
			if (Transform* transform = world.GetComponent<Transform>(child)) {
				const Matrix2x3 parentWorld = parent.IsValid() ? ComputeWorldMatrix(world, parent) : Matrix2x3::Identity();
				const Matrix2x3 local = parentWorld.Inverse() * childWorld;
				transform->Position = local.GetTranslation();
				transform->Rotation = local.GetRotation();
				transform->Scale = local.GetScale();
			}
		}
		return true;
	}

	Entity Hierarchy::GetParent(const World& world, Entity entity)
	{
		const Parent* link = world.GetComponent<Parent>(entity);
		return link && world.IsAlive(link->Value) ? link->Value : Entity{};
	}

	std::span<const Entity> Hierarchy::GetChildren(const World& world, Entity entity)
	{
		const Children* children = world.GetComponent<Children>(entity);
		return children ? std::span<const Entity>{ children->Entities } : std::span<const Entity>{};
	}

	void Hierarchy::SetSiblingIndex(World& world, Entity entity, int index)
	{
		const Entity parent = GetParent(world, entity);
		Children* children = parent.IsValid() ? world.GetComponent<Children>(parent) : nullptr;
		if (!children) {
			return;
		}
		std::vector<Entity>& list = children->Entities;
		const auto it = std::find(list.begin(), list.end(), entity);
		if (it == list.end()) {
			return;
		}
		list.erase(it);
		const size_t position = std::min(static_cast<size_t>(std::max(index, 0)), list.size());
		list.insert(list.begin() + static_cast<std::ptrdiff_t>(position), entity);
	}

	Entity Hierarchy::GetRoot(const World& world, Entity entity)
	{
		if (!world.IsAlive(entity)) {
			return {};
		}
		Entity root = entity;
		for (Entity parent = GetParent(world, root); parent.IsValid(); parent = GetParent(world, root)) {
			root = parent;
		}
		return root;
	}

	void Hierarchy::DestroyWithChildren(World& world, Entity entity)
	{
		if (!world.IsAlive(entity)) {
			return;
		}
		const Entity parent = GetParent(world, entity);
		if (parent.IsValid()) {
			RemoveFromChildren(world, parent, entity);
		}

		// Collect the whole subtree first: destroying moves rows and invalidates component pointers.
		std::vector<Entity> subtree{ entity };
		for (size_t i = 0; i < subtree.size(); ++i) {
			for (Entity child : GetChildren(world, subtree[i])) {
				if (world.IsAlive(child)) {
					subtree.push_back(child);
				}
			}
		}
		for (Entity doomed : subtree) {
			world.Destroy(doomed);
		}
	}
}
