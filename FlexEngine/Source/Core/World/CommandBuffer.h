#pragma once
#include "Core/Core.h"
#include "Core/World/ComponentRegistery.h"
#include "Core/World/Entity.h"
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace Flex
{
	class World;

	// Records structural changes (spawn, destroy, add/remove component) so they can be
	// made safely while iterating, and applies them later in the order they were recorded.
	//
	//     world.Each<Bullet>([&](Entity bullet, Bullet& b) {
	//         if (b.Life <= 0.0f) {
	//             world.GetCommands().Destroy(bullet);
	//         }
	//     });
	//     world.ApplyCommands();   // end of frame
	//
	// Commands on an entity that is no longer alive when they're applied are ignored,
	// so destroying the same bullet twice in one frame is fine.
	// Not thread-safe (one buffer per thread is a later step).
	class FLEX_API CommandBuffer
	{
	public:
		explicit CommandBuffer(World& world);
		~CommandBuffer();

		CommandBuffer(const CommandBuffer&) = delete;
		CommandBuffer& operator=(const CommandBuffer&) = delete;

		// Reserves the entity now, so later commands can use the handle.
		// It becomes alive when the commands are applied.
		template<typename... Ts>
			requires (IsComponent<std::decay_t<Ts>> && ...)
		Entity Spawn(Ts&&... components)
		{
			static_assert(DistinctTypes<std::decay_t<Ts>...>::value, "Spawn: a component type appears twice.");
			const Entity entity = ReserveEntity();
			if constexpr (sizeof...(Ts) == 0) {
				RecordSpawn(entity, nullptr, nullptr, 0);
			} else {
				const ComponentTypeInfo* types[] = { &GetComponentType<std::decay_t<Ts>>()... };
				void* payloads[] = { StorePayload<std::decay_t<Ts>>(std::forward<Ts>(components))... };
				RecordSpawn(entity, types, payloads, static_cast<uint32_t>(sizeof...(Ts)));
			}
			return entity;
		}

		void Destroy(Entity entity);

		// Adds the component, or replaces its value when the entity already has it.
		template<IsComponent T>
		void AddComponent(Entity entity, T component = {})
		{
			RecordAddComponent(entity, GetComponentType<T>(), StorePayload<T>(std::move(component)));
		}

		template<IsComponent T>
		void RemoveComponent(Entity entity)
		{
			RecordRemoveComponent(entity, GetComponentType<T>());
		}

		// Runs function(world) when the commands are applied, in order with the other commands.
		// For changes that take several steps, e.g. Hierarchy::SetParent inside World::Each.
		// The function may record more commands; they run in the same Apply.
		void Defer(std::function<void(World&)> function);

		// Applies and clears every recorded command.
		void Apply();

		bool IsEmpty() const { return Commands.empty(); }
		uint32_t GetCommandCount() const { return static_cast<uint32_t>(Commands.size()); }

	private:
		enum class CommandType : uint8_t
		{
			Spawn,
			Destroy,
			AddComponent,
			RemoveComponent,
			Deferred
		};

		struct Command
		{
			CommandType Type = CommandType::Destroy;
			Entity Target;
			const ComponentTypeInfo* Component = nullptr; // AddComponent, RemoveComponent
			void* Payload = nullptr;                      // AddComponent
			uint32_t FirstSpawnComponent = 0;             // Spawn: range in SpawnTypes/SpawnPayloads
			uint32_t SpawnComponentCount = 0;
			uint32_t DeferredFunction = 0;                // Deferred: index in DeferredFunctions
		};

		struct PayloadBlock
		{
			std::byte* Data = nullptr;
			size_t Size = 0;
		};

		template<typename T, typename Arg>
		void* StorePayload(Arg&& value)
		{
			void* storage = AllocatePayload(sizeof(T), alignof(T));
			::new (storage) T(std::forward<Arg>(value));
			return storage;
		}

		Entity ReserveEntity();
		void* AllocatePayload(size_t size, size_t alignment);
		void RecordSpawn(Entity entity, const ComponentTypeInfo* const* types, void* const* payloads, uint32_t count);
		void RecordAddComponent(Entity entity, const ComponentTypeInfo& type, void* payload);
		void RecordRemoveComponent(Entity entity, const ComponentTypeInfo& type);
		void DestroyPendingPayloads();
		void Reset();

		World& OwnerWorld;
		std::vector<Command> Commands;
		std::vector<const ComponentTypeInfo*> SpawnTypes; // sorted by id per spawn
		std::vector<void*> SpawnPayloads;
		std::vector<std::function<void(World&)>> DeferredFunctions;

		// Payloads live in fixed blocks so they never move once constructed.
		std::vector<PayloadBlock> Blocks;
		size_t CurrentBlock = 0;
		size_t BlockOffset = 0;
	};
}
