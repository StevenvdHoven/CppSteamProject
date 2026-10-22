#include "Core/World/CommandBuffer.h"
#include "Core/World/World.h"
#include <algorithm>
#include <new>

namespace Flex
{
	namespace
	{
		constexpr size_t PayloadBlockSize = 64 * 1024;
		constexpr size_t PayloadBlockAlignment = 64;
	}

	CommandBuffer::CommandBuffer(World& world)
		: OwnerWorld{ world }
	{
	}

	CommandBuffer::~CommandBuffer()
	{
		DestroyPendingPayloads();
		for (PayloadBlock& block : Blocks) {
			::operator delete(block.Data, std::align_val_t{ PayloadBlockAlignment });
		}
	}

	void CommandBuffer::Destroy(Entity entity)
	{
		Command& command = Commands.emplace_back();
		command.Type = CommandType::Destroy;
		command.Target = entity;
	}

	void CommandBuffer::Apply()
	{
		// Applying can't record new commands (nothing user-side runs), so a plain loop is safe.
		for (Command& command : Commands) {
			switch (command.Type) {
			case CommandType::Spawn:
				OwnerWorld.SpawnReserved(command.Target, SpawnTypes.data() + command.FirstSpawnComponent,
					SpawnPayloads.data() + command.FirstSpawnComponent, command.SpawnComponentCount);
				break;
			case CommandType::Destroy:
				OwnerWorld.Destroy(command.Target);
				break;
			case CommandType::AddComponent:
				OwnerWorld.AddComponentFromPayload(command.Target, *command.Component, command.Payload);
				break;
			case CommandType::RemoveComponent:
				OwnerWorld.RemoveComponentById(command.Target, command.Component->Id);
				break;
			}
		}
		Reset();
	}

	Entity CommandBuffer::ReserveEntity()
	{
		return OwnerWorld.AllocateEntity();
	}

	void* CommandBuffer::AllocatePayload(size_t size, size_t alignment)
	{
		FLEX_ASSERT(alignment <= PayloadBlockAlignment, "Component alignment above 64 isn't supported by CommandBuffer.");
		while (true) {
			if (CurrentBlock < Blocks.size()) {
				PayloadBlock& block = Blocks[CurrentBlock];
				const size_t offset = (BlockOffset + alignment - 1) & ~(alignment - 1);
				if (offset + size <= block.Size) {
					BlockOffset = offset + size;
					return block.Data + offset;
				}
				++CurrentBlock;
				BlockOffset = 0;
				continue;
			}
			const size_t blockSize = std::max(PayloadBlockSize, size);
			Blocks.push_back({ static_cast<std::byte*>(::operator new(blockSize, std::align_val_t{ PayloadBlockAlignment })), blockSize });
		}
	}

	void CommandBuffer::RecordSpawn(Entity entity, const ComponentTypeInfo* const* types, void* const* payloads, uint32_t count)
	{
		Command& command = Commands.emplace_back();
		command.Type = CommandType::Spawn;
		command.Target = entity;
		command.FirstSpawnComponent = static_cast<uint32_t>(SpawnTypes.size());
		command.SpawnComponentCount = count;

		// Store this spawn's components sorted by id: that's the archetype's column order.
		const size_t first = SpawnTypes.size();
		for (uint32_t i = 0; i < count; ++i) {
			SpawnTypes.push_back(types[i]);
			SpawnPayloads.push_back(payloads[i]);
		}
		for (size_t i = first + 1; i < SpawnTypes.size(); ++i) {
			for (size_t j = i; j > first && SpawnTypes[j - 1]->Id > SpawnTypes[j]->Id; --j) {
				std::swap(SpawnTypes[j - 1], SpawnTypes[j]);
				std::swap(SpawnPayloads[j - 1], SpawnPayloads[j]);
			}
		}
	}

	void CommandBuffer::RecordAddComponent(Entity entity, const ComponentTypeInfo& type, void* payload)
	{
		Command& command = Commands.emplace_back();
		command.Type = CommandType::AddComponent;
		command.Target = entity;
		command.Component = &type;
		command.Payload = payload;
	}

	void CommandBuffer::RecordRemoveComponent(Entity entity, const ComponentTypeInfo& type)
	{
		Command& command = Commands.emplace_back();
		command.Type = CommandType::RemoveComponent;
		command.Target = entity;
		command.Component = &type;
	}

	void CommandBuffer::DestroyPendingPayloads()
	{
		for (Command& command : Commands) {
			if (command.Type == CommandType::AddComponent) {
				command.Component->DestroyObject(command.Payload);
			}
		}
		for (size_t i = 0; i < SpawnTypes.size(); ++i) {
			SpawnTypes[i]->DestroyObject(SpawnPayloads[i]);
		}
	}

	void CommandBuffer::Reset()
	{
		Commands.clear();
		SpawnTypes.clear();
		SpawnPayloads.clear();
		CurrentBlock = 0;
		BlockOffset = 0;
	}
}
