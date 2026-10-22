#include "Core/World/Archetype.h"
#include <new>

namespace Flex
{
	namespace
	{
		// Columns are at least cache-line aligned, which also suits SIMD loops.
		std::align_val_t GetColumnAlignment(const ComponentTypeInfo& type)
		{
			return std::align_val_t{ std::max<size_t>(type.Alignment, 64) };
		}
	}

	Archetype::Archetype(std::vector<const ComponentTypeInfo*> sortedTypes)
	{
		ComponentIds.reserve(sortedTypes.size());
		Columns.reserve(sortedTypes.size());
		for (const ComponentTypeInfo* type : sortedTypes) {
			FLEX_ASSERT(ComponentIds.empty() || ComponentIds.back() < type->Id, "Archetype types must be sorted and unique.");
			ComponentIds.push_back(type->Id);
			Columns.push_back({ type, type->Size, nullptr });
		}
	}

	Archetype::~Archetype()
	{
		for (Column& column : Columns) {
			if (column.Type->Destruct) {
				for (uint32_t row = 0; row < Count; ++row) {
					column.Type->Destruct(column.Data + static_cast<size_t>(row) * column.Size);
				}
			}
			if (column.Data) {
				::operator delete(column.Data, GetColumnAlignment(*column.Type));
			}
		}
	}

	void Archetype::Reserve(uint32_t capacity)
	{
		if (capacity <= Capacity) {
			return;
		}

		for (Column& column : Columns) {
			std::byte* data = static_cast<std::byte*>(::operator new(static_cast<size_t>(capacity) * column.Size, GetColumnAlignment(*column.Type)));
			if (column.Data) {
				if (column.Type->Relocate) {
					for (uint32_t row = 0; row < Count; ++row) {
						const size_t offset = static_cast<size_t>(row) * column.Size;
						column.Type->Relocate(data + offset, column.Data + offset);
					}
				} else if (Count > 0) {
					std::memcpy(data, column.Data, static_cast<size_t>(Count) * column.Size);
				}
				::operator delete(column.Data, GetColumnAlignment(*column.Type));
			}
			column.Data = data;
		}
		Entities.reserve(capacity);
		Capacity = capacity;
	}

	uint32_t Archetype::AddRow(Entity entity)
	{
		if (Count == Capacity) {
			Reserve(std::max<uint32_t>(16, Capacity * 2));
		}
		Entities.push_back(entity);
		return Count++;
	}

	Entity Archetype::RemoveVacatedRow(uint32_t row)
	{
		FLEX_ASSERT(row < Count, "Row out of range.");
		const uint32_t last = Count - 1;
		Entity moved{};
		if (row != last) {
			for (Column& column : Columns) {
				column.Type->RelocateObject(column.Data + static_cast<size_t>(row) * column.Size, column.Data + static_cast<size_t>(last) * column.Size);
			}
			moved = Entities[last];
			Entities[row] = moved;
		}
		Entities.pop_back();
		--Count;
		return moved;
	}

	Entity Archetype::DestroyRow(uint32_t row)
	{
		for (Column& column : Columns) {
			column.Type->DestroyObject(column.Data + static_cast<size_t>(row) * column.Size);
		}
		return RemoveVacatedRow(row);
	}
}
