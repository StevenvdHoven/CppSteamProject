#pragma once
#include "Core/Core.h"
#include "Core/World/ComponentRegistery.h"
#include "Core/World/Entity.h"
#include <algorithm>
#include <cstddef>
#include <span>
#include <unordered_map>
#include <vector>

namespace Flex
{
	// Table of all entities that have exactly the same set of components.
	// Every component type gets one contiguous column (struct of arrays), plus one
	// column of Entity handles. Rows are packed: removing a row moves the last row
	// into the hole, so row order is NOT stable.
	//
	// Only World changes an archetype; the public part is read access for queries.
	class FLEX_API Archetype
	{
	public:
		// sortedTypes must be sorted by Id without duplicates.
		explicit Archetype(std::vector<const ComponentTypeInfo*> sortedTypes);
		~Archetype();

		Archetype(const Archetype&) = delete;
		Archetype& operator=(const Archetype&) = delete;

		const std::vector<ComponentId>& GetComponentIds() const { return ComponentIds; }
		uint32_t GetCount() const { return Count; }
		uint32_t GetCapacity() const { return Capacity; }
		const Entity* GetEntities() const { return Entities.data(); }

		// Column of a component, or -1 when this archetype doesn't have it.
		int32_t FindColumn(ComponentId id) const
		{
			for (size_t column = 0; column < ComponentIds.size(); ++column) {
				if (ComponentIds[column] == id) {
					return static_cast<int32_t>(column);
				}
			}
			return -1;
		}

		void* GetColumnData(int32_t column) const { return Columns[column].Data; }

		void* GetElement(int32_t column, uint32_t row) const
		{
			return Columns[column].Data + static_cast<size_t>(row) * Columns[column].Size;
		}

	private:
		friend class World;

		struct Column
		{
			const ComponentTypeInfo* Type = nullptr;
			uint32_t Size = 0;
			std::byte* Data = nullptr;
		};

		void Reserve(uint32_t capacity);

		// Appends a row for entity. Its components are NOT constructed; the caller does that.
		uint32_t AddRow(Entity entity);

		// The components in row were already destroyed or moved out: fills the hole with
		// the last row. Returns the entity that moved into row, or Entity{} if none did.
		Entity RemoveVacatedRow(uint32_t row);

		// Destroys the components in row, then removes it like RemoveVacatedRow.
		Entity DestroyRow(uint32_t row);

		std::vector<ComponentId> ComponentIds;
		std::vector<Column> Columns;
		std::vector<Entity> Entities;
		uint32_t Count = 0;
		uint32_t Capacity = 0;

		// Cached archetype moves: "this + component" and "this - component".
		std::unordered_map<ComponentId, Archetype*> AddEdges;
		std::unordered_map<ComponentId, Archetype*> RemoveEdges;
	};

	// Hash/equality for looking archetypes up by their sorted component ids,
	// with a span so lookups don't allocate.
	struct ArchetypeKeyHash
	{
		using is_transparent = void;

		size_t operator()(std::span<const ComponentId> ids) const noexcept
		{
			size_t hash = ids.size();
			for (ComponentId id : ids) {
				hash ^= id + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
			}
			return hash;
		}
	};

	struct ArchetypeKeyEqual
	{
		using is_transparent = void;

		bool operator()(std::span<const ComponentId> a, std::span<const ComponentId> b) const noexcept
		{
			return std::ranges::equal(a, b);
		}
	};
}
