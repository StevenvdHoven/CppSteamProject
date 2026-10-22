#include "Core/World/World.h"
#include <algorithm>

namespace Flex
{
	World::World()
		: Commands{ *this }
	{
		FindOrCreateArchetype({});
	}

	World::~World() = default;

	// -----------------------
	// Entities
	// -----------------------
	void World::Destroy(Entity entity)
	{
		AssertNotIterating();
		if (!IsAlive(entity)) {
			return;
		}
		EntityRecord& record = Records[entity.Index];
		const Entity moved = record.Owner->DestroyRow(record.Row);
		if (moved.IsValid()) {
			Records[moved.Index].Row = record.Row;
		}
		FreeRecord(entity.Index);
	}

	bool World::IsAlive(Entity entity) const
	{
		return entity.Index < Records.size()
			&& Records[entity.Index].Generation == entity.Generation
			&& Records[entity.Index].Owner != nullptr;
	}

	Entity World::AllocateEntity()
	{
		uint32_t index;
		if (!FreeIndices.empty()) {
			index = FreeIndices.back();
			FreeIndices.pop_back();
		} else {
			index = static_cast<uint32_t>(Records.size());
			Records.emplace_back();
		}
		return { index, Records[index].Generation };
	}

	uint32_t World::PlaceEntity(Entity entity, Archetype& archetype)
	{
		AssertNotIterating();
		EntityRecord& record = Records[entity.Index];
		record.Owner = &archetype;
		record.Row = archetype.AddRow(entity);
		++AliveCount;
		return record.Row;
	}

	uint32_t World::BeginSpawnBatch(Archetype& archetype, uint32_t count)
	{
		AssertNotIterating();
		archetype.Reserve(archetype.GetCount() + count);
		if (count > FreeIndices.size()) {
			Records.reserve(Records.size() + (count - FreeIndices.size()));
		}
		const uint32_t firstRow = archetype.GetCount();
		for (uint32_t i = 0; i < count; ++i) {
			PlaceEntity(AllocateEntity(), archetype);
		}
		return firstRow;
	}

	Entity World::SpawnEmpty()
	{
		const Entity entity = AllocateEntity();
		PlaceEntity(entity, *Archetypes[0]);
		return entity;
	}

	void World::FreeRecord(uint32_t index)
	{
		EntityRecord& record = Records[index];
		record.Owner = nullptr;
		record.Row = 0;
		if (++record.Generation == 0) {
			record.Generation = 1;
		}
		FreeIndices.push_back(index);
		--AliveCount;
	}

	// -----------------------
	// Archetypes
	// -----------------------
	Archetype& World::FindOrCreateArchetype(std::span<const ComponentTypeInfo*> types)
	{
		std::sort(types.begin(), types.end(), [](const ComponentTypeInfo* a, const ComponentTypeInfo* b) { return a->Id < b->Id; });

		constexpr size_t StackIdCount = 32;
		ComponentId stackIds[StackIdCount];
		std::vector<ComponentId> heapIds;
		ComponentId* ids = stackIds;
		if (types.size() > StackIdCount) {
			heapIds.resize(types.size());
			ids = heapIds.data();
		}
		for (size_t i = 0; i < types.size(); ++i) {
			ids[i] = types[i]->Id;
			FLEX_ASSERT(i == 0 || ids[i - 1] != ids[i], "A component type appears twice.");
		}

		const std::span<const ComponentId> key{ ids, types.size() };
		if (auto it = ArchetypeLookup.find(key); it != ArchetypeLookup.end()) {
			return *it->second;
		}

		Archetype& archetype = *Archetypes.emplace_back(std::make_unique<Archetype>(std::vector<const ComponentTypeInfo*>(types.begin(), types.end())));
		ArchetypeLookup.emplace(std::vector<ComponentId>(key.begin(), key.end()), &archetype);
		return archetype;
	}

	Archetype& World::GetArchetypeWithAdded(Archetype& source, const ComponentTypeInfo& type)
	{
		if (auto it = source.AddEdges.find(type.Id); it != source.AddEdges.end()) {
			return *it->second;
		}
		std::vector<const ComponentTypeInfo*> types;
		types.reserve(source.Columns.size() + 1);
		for (const Archetype::Column& column : source.Columns) {
			types.push_back(column.Type);
		}
		types.push_back(&type);
		Archetype& target = FindOrCreateArchetype(types);
		source.AddEdges[type.Id] = &target;
		target.RemoveEdges[type.Id] = &source;
		return target;
	}

	Archetype& World::GetArchetypeWithRemoved(Archetype& source, ComponentId id)
	{
		if (auto it = source.RemoveEdges.find(id); it != source.RemoveEdges.end()) {
			return *it->second;
		}
		std::vector<const ComponentTypeInfo*> types;
		types.reserve(source.Columns.size());
		for (const Archetype::Column& column : source.Columns) {
			if (column.Type->Id != id) {
				types.push_back(column.Type);
			}
		}
		Archetype& target = FindOrCreateArchetype(types);
		source.RemoveEdges[id] = &target;
		target.AddEdges[id] = &source;
		return target;
	}

	void World::MoveEntity(EntityRecord& record, Archetype& target)
	{
		Archetype& source = *record.Owner;
		const uint32_t sourceRow = record.Row;
		const uint32_t targetRow = target.AddRow(source.Entities[sourceRow]);

		// Both column lists are sorted by id: move shared components, destroy the rest.
		size_t targetColumn = 0;
		for (size_t sourceColumn = 0; sourceColumn < source.Columns.size(); ++sourceColumn) {
			const ComponentId id = source.ComponentIds[sourceColumn];
			while (targetColumn < target.ComponentIds.size() && target.ComponentIds[targetColumn] < id) {
				++targetColumn;
			}
			const Archetype::Column& column = source.Columns[sourceColumn];
			void* sourceElement = column.Data + static_cast<size_t>(sourceRow) * column.Size;
			if (targetColumn < target.ComponentIds.size() && target.ComponentIds[targetColumn] == id) {
				column.Type->RelocateObject(target.GetElement(static_cast<int32_t>(targetColumn), targetRow), sourceElement);
			} else {
				column.Type->DestroyObject(sourceElement);
			}
		}

		const Entity moved = source.RemoveVacatedRow(sourceRow);
		if (moved.IsValid()) {
			Records[moved.Index].Row = sourceRow;
		}
		record.Owner = &target;
		record.Row = targetRow;
	}

	// -----------------------
	// Components
	// -----------------------
	std::span<const ComponentId> World::GetComponentIds(Entity entity) const
	{
		if (!IsAlive(entity)) {
			return {};
		}
		return Records[entity.Index].Owner->GetComponentIds();
	}

	void* World::GetComponentData(Entity entity, ComponentId id) const
	{
		if (!IsAlive(entity)) {
			return nullptr;
		}
		const EntityRecord& record = Records[entity.Index];
		const int32_t column = record.Owner->FindColumn(id);
		return column >= 0 ? record.Owner->GetElement(column, record.Row) : nullptr;
	}

	void* World::PrepareAddComponent(Entity entity, const ComponentTypeInfo& type, bool& bOutAdded)
	{
		AssertNotIterating();
		bOutAdded = false;
		if (!IsAlive(entity)) {
			return nullptr;
		}
		EntityRecord& record = Records[entity.Index];
		if (const int32_t column = record.Owner->FindColumn(type.Id); column >= 0) {
			return record.Owner->GetElement(column, record.Row);
		}
		Archetype& target = GetArchetypeWithAdded(*record.Owner, type);
		MoveEntity(record, target);
		bOutAdded = true;
		return target.GetElement(target.FindColumn(type.Id), record.Row);
	}

	void* World::AddComponentById(Entity entity, ComponentId id)
	{
		const ComponentTypeInfo& type = ComponentRegistery::GetInfo(id);
		if (!type.DefaultConstruct || !IsAlive(entity)) {
			return nullptr;
		}
		bool bAdded = false;
		void* storage = PrepareAddComponent(entity, type, bAdded);
		if (storage && bAdded) {
			type.DefaultConstruct(storage);
		}
		return storage;
	}

	bool World::RemoveComponentById(Entity entity, ComponentId id)
	{
		AssertNotIterating();
		if (!IsAlive(entity)) {
			return false;
		}
		EntityRecord& record = Records[entity.Index];
		if (record.Owner->FindColumn(id) < 0) {
			return false;
		}
		MoveEntity(record, GetArchetypeWithRemoved(*record.Owner, id));
		return true;
	}

	// -----------------------
	// Deferred changes
	// -----------------------
	void World::ApplyCommands()
	{
		AssertNotIterating();
		Commands.Apply();
	}

	void World::SpawnReserved(Entity entity, const ComponentTypeInfo* const* types, void* const* payloads, uint32_t count)
	{
		const bool bReserved = entity.Index < Records.size()
			&& Records[entity.Index].Generation == entity.Generation
			&& Records[entity.Index].Owner == nullptr;
		if (!bReserved) {
			for (uint32_t i = 0; i < count; ++i) {
				types[i]->DestroyObject(payloads[i]);
			}
			return;
		}

		// FindOrCreateArchetype sorts in place, so give it a copy (on the stack when small).
		constexpr uint32_t StackTypeCount = 32;
		const ComponentTypeInfo* stackTypes[StackTypeCount];
		std::vector<const ComponentTypeInfo*> heapTypes;
		const ComponentTypeInfo** typesCopy = stackTypes;
		if (count > StackTypeCount) {
			heapTypes.resize(count);
			typesCopy = heapTypes.data();
		}
		std::copy(types, types + count, typesCopy);
		Archetype& archetype = FindOrCreateArchetype({ typesCopy, count });
		const uint32_t row = PlaceEntity(entity, archetype);
		// types are already sorted by id, so component i goes to column i.
		for (uint32_t i = 0; i < count; ++i) {
			types[i]->RelocateObject(archetype.GetElement(static_cast<int32_t>(i), row), payloads[i]);
		}
	}

	void World::AddComponentFromPayload(Entity entity, const ComponentTypeInfo& type, void* payload)
	{
		bool bAdded = false;
		void* storage = PrepareAddComponent(entity, type, bAdded);
		if (!storage) {
			type.DestroyObject(payload);
			return;
		}
		if (!bAdded) {
			type.DestroyObject(storage);
		}
		type.RelocateObject(storage, payload);
	}

	void World::AssertNotIterating() const
	{
		FLEX_ASSERT(IterationDepth == 0, "Structural change inside World::Each. Use World::GetCommands() instead.");
	}
}
