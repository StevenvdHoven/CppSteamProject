#pragma once
#include "Core/Core.h"
#include "Core/World/Archetype.h"
#include "Core/World/CommandBuffer.h"
#include "Core/World/ComponentRegistery.h"
#include "Core/World/Entity.h"
#include <memory>
#include <span>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Flex
{
	// A set of entities and their components, stored per archetype (see Archetype.h).
	//
	//     World world;
	//     Entity e = world.Spawn(Position{ { 10, 20 } }, Velocity{ { 1, 0 } });
	//     world.Each<Position, const Velocity>([&](Position& p, const Velocity& v) {
	//         p.Value += v.Value * dt;
	//     });
	//
	// Structural changes (Spawn, SpawnBatch, Destroy, AddComponent, RemoveComponent)
	// are not allowed inside Each: record them in GetCommands() and call ApplyCommands().
	// Changing component values inside Each is always fine.
	// Not thread-safe.
	class FLEX_API World
	{
	public:
		World();
		~World();

		World(const World&) = delete;
		World& operator=(const World&) = delete;

		// -----------------------
		// Entities
		// -----------------------
		template<typename... Ts>
			requires (IsComponent<std::decay_t<Ts>> && ...)
		Entity Spawn(Ts&&... components)
		{
			static_assert(DistinctTypes<std::decay_t<Ts>...>::value, "Spawn: a component type appears twice.");
			if constexpr (sizeof...(Ts) == 0) {
				return SpawnEmpty();
			} else {
				const ComponentTypeInfo* types[] = { &GetComponentType<std::decay_t<Ts>>()... };
				Archetype& archetype = FindOrCreateArchetype(types);
				const Entity entity = AllocateEntity();
				const uint32_t row = PlaceEntity(entity, archetype);
				(::new (archetype.GetElement(archetype.FindColumn(GetComponentId<std::decay_t<Ts>>()), row)) std::decay_t<Ts>(std::forward<Ts>(components)), ...);
				return entity;
			}
		}

		// Spawns count entities with default-constructed Ts, reusing freed slots first,
		// then calls init for each: init(Entity, Ts&...) or init(Ts&...).
		template<IsComponent... Ts, typename Func>
		void SpawnBatch(uint32_t count, Func&& init)
		{
			static_assert(sizeof...(Ts) > 0, "SpawnBatch needs at least one component type.");
			static_assert(DistinctTypes<Ts...>::value, "SpawnBatch: a component type appears twice.");
			static_assert((std::is_default_constructible_v<Ts> && ...), "SpawnBatch: components must be default-constructible.");
			if (count == 0) {
				return;
			}
			const ComponentTypeInfo* types[] = { &GetComponentType<Ts>()... };
			Archetype& archetype = FindOrCreateArchetype(types);
			const uint32_t firstRow = BeginSpawnBatch(archetype, count);
			const int32_t columns[] = { archetype.FindColumn(GetComponentId<Ts>())... };
			SpawnBatchRows<Ts...>(archetype, firstRow, count, columns, init, std::index_sequence_for<Ts...>{});
		}

		// Destroying an entity that isn't alive does nothing.
		void Destroy(Entity entity);
		bool IsAlive(Entity entity) const;
		uint32_t GetEntityCount() const { return AliveCount; }

		// -----------------------
		// Components
		// -----------------------
		// Adds the component, or replaces its value when the entity already has it.
		// Returns nullptr when the entity isn't alive.
		template<IsComponent T>
		T* AddComponent(Entity entity, T component = {})
		{
			bool bAdded = false;
			void* storage = PrepareAddComponent(entity, GetComponentType<T>(), bAdded);
			if (!storage) {
				return nullptr;
			}
			if (!bAdded) {
				std::destroy_at(static_cast<T*>(storage));
			}
			return ::new (storage) T(std::move(component));
		}

		// Returns false when the entity isn't alive or doesn't have the component.
		template<IsComponent T>
		bool RemoveComponent(Entity entity)
		{
			return RemoveComponentById(entity, GetComponentId<T>());
		}

		// nullptr when the entity isn't alive or doesn't have the component.
		// The pointer is valid until the next structural change.
		template<IsComponent T>
		T* GetComponent(Entity entity)
		{
			return static_cast<T*>(GetComponentData(entity, GetComponentId<T>()));
		}

		template<IsComponent T>
		const T* GetComponent(Entity entity) const
		{
			return static_cast<const T*>(GetComponentData(entity, GetComponentId<T>()));
		}

		template<IsComponent T>
		bool HasComponent(Entity entity) const
		{
			return GetComponentData(entity, GetComponentId<T>()) != nullptr;
		}

		// The entity's component types (sorted by id); empty when it isn't alive.
		// Valid until the next structural change.
		std::span<const ComponentId> GetComponentIds(Entity entity) const;

		// Type-erased versions for code that only knows a ComponentId (Inspector, scene files).
		void* GetComponentById(Entity entity, ComponentId id) const { return GetComponentData(entity, id); }
		// Default-constructs the component, or returns the existing one. nullptr when the entity
		// isn't alive or the type has no default constructor.
		void* AddComponentById(Entity entity, ComponentId id);
		// Returns false when the entity isn't alive or doesn't have the component.
		bool RemoveComponentById(Entity entity, ComponentId id);

		// -----------------------
		// Queries
		// -----------------------
		// Calls func for every entity that has all of Ts: func(Ts&...) or func(Entity, Ts&...).
		// Use const T for read-only access. Inlines into the caller: FlexCore isn't called per entity.
		template<typename... Ts, typename Func>
			requires (IsComponent<std::remove_const_t<Ts>> && ...)
		void Each(Func&& func)
		{
			static_assert(sizeof...(Ts) > 0, "Each needs at least one component type.");
			static_assert(DistinctTypes<std::remove_const_t<Ts>...>::value, "Each: a component type appears twice.");
			const ComponentId ids[] = { GetComponentId<std::remove_const_t<Ts>>()... };
			IterationScope scope{ *this };
			for (const std::unique_ptr<Archetype>& archetype : Archetypes) {
				if (archetype->GetCount() == 0) {
					continue;
				}
				int32_t columns[sizeof...(Ts)];
				bool bMatches = true;
				for (size_t i = 0; i < sizeof...(Ts); ++i) {
					columns[i] = archetype->FindColumn(ids[i]);
					bMatches = bMatches && columns[i] >= 0;
				}
				if (bMatches) {
					EachInArchetype<Ts...>(*archetype, columns, func, std::index_sequence_for<Ts...>{});
				}
			}
		}

		// -----------------------
		// Deferred changes
		// -----------------------
		CommandBuffer& GetCommands() { return Commands; }
		void ApplyCommands();

		// Read-only view for statistics and debug tools.
		const std::vector<std::unique_ptr<Archetype>>& GetArchetypes() const { return Archetypes; }

	private:
		friend class CommandBuffer;

		struct EntityRecord
		{
			Archetype* Owner = nullptr; // nullptr = free, or reserved by a CommandBuffer
			uint32_t Row = 0;
			uint32_t Generation = 1;
		};

		struct IterationScope
		{
			World& Owner;
			explicit IterationScope(World& owner) : Owner{ owner } { ++Owner.IterationDepth; }
			~IterationScope() { --Owner.IterationDepth; }
			IterationScope(const IterationScope&) = delete;
			IterationScope& operator=(const IterationScope&) = delete;
		};

		template<typename... Ts, typename Func, size_t... I>
		static void EachInArchetype(Archetype& archetype, const int32_t* columns, Func& func, std::index_sequence<I...>)
		{
			const uint32_t count = archetype.GetCount();
			const std::tuple<Ts*...> data{ static_cast<Ts*>(archetype.GetColumnData(columns[I]))... };
			if constexpr (std::is_invocable_v<Func&, Entity, Ts&...>) {
				const Entity* entities = archetype.GetEntities();
				for (uint32_t row = 0; row < count; ++row) {
					func(entities[row], std::get<I>(data)[row]...);
				}
			} else {
				static_assert(std::is_invocable_v<Func&, Ts&...>, "Each: func must take (Ts&...) or (Entity, Ts&...).");
				for (uint32_t row = 0; row < count; ++row) {
					func(std::get<I>(data)[row]...);
				}
			}
		}

		template<typename... Ts, typename Func, size_t... I>
		static void SpawnBatchRows(Archetype& archetype, uint32_t firstRow, uint32_t count, const int32_t* columns, Func& init, std::index_sequence<I...>)
		{
			const std::tuple<Ts*...> data{ static_cast<Ts*>(archetype.GetColumnData(columns[I]))... };
			const Entity* entities = archetype.GetEntities();
			for (uint32_t row = firstRow; row < firstRow + count; ++row) {
				(::new (std::get<I>(data) + row) Ts(), ...);
				if constexpr (std::is_invocable_v<Func&, Entity, Ts&...>) {
					init(entities[row], std::get<I>(data)[row]...);
				} else {
					static_assert(std::is_invocable_v<Func&, Ts&...>, "SpawnBatch: init must take (Ts&...) or (Entity, Ts&...).");
					init(std::get<I>(data)[row]...);
				}
			}
		}

		// Sorts types by id (in place) and returns the archetype for exactly that set.
		Archetype& FindOrCreateArchetype(std::span<const ComponentTypeInfo*> types);
		Archetype& GetArchetypeWithAdded(Archetype& source, const ComponentTypeInfo& type);
		Archetype& GetArchetypeWithRemoved(Archetype& source, ComponentId id);

		// A new handle whose slot isn't in any archetype yet ("reserved").
		Entity AllocateEntity();
		uint32_t PlaceEntity(Entity entity, Archetype& archetype);
		uint32_t BeginSpawnBatch(Archetype& archetype, uint32_t count);
		Entity SpawnEmpty();
		void MoveEntity(EntityRecord& record, Archetype& target);
		void FreeRecord(uint32_t index);

		void* GetComponentData(Entity entity, ComponentId id) const;
		void* PrepareAddComponent(Entity entity, const ComponentTypeInfo& type, bool& bOutAdded);

		// Used by CommandBuffer::Apply. Payloads are always consumed (moved in or destroyed).
		void SpawnReserved(Entity entity, const ComponentTypeInfo* const* types, void* const* payloads, uint32_t count);
		void AddComponentFromPayload(Entity entity, const ComponentTypeInfo& type, void* payload);

		void AssertNotIterating() const;

		std::vector<std::unique_ptr<Archetype>> Archetypes; // [0] is the empty archetype
		std::unordered_map<std::vector<ComponentId>, Archetype*, ArchetypeKeyHash, ArchetypeKeyEqual> ArchetypeLookup;
		std::vector<EntityRecord> Records;
		std::vector<uint32_t> FreeIndices;
		uint32_t AliveCount = 0;
		uint32_t IterationDepth = 0;
		CommandBuffer Commands;
	};
}
