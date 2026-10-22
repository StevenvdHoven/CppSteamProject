#pragma once
#include <compare>
#include <cstdint>
#include <functional>

namespace Flex
{
	// Handle to an entity in a World: a slot index plus the generation of that slot.
	// Destroying an entity bumps its slot's generation, so old handles to a reused slot
	// stop being alive instead of pointing at the new entity.
	// Entity{} (generation 0) is "no entity"; real generations start at 1.
	struct Entity
	{
		uint32_t Index = 0;
		uint32_t Generation = 0;

		constexpr bool IsValid() const { return Generation != 0; }

		constexpr auto operator<=>(const Entity&) const = default;
	};
}

template<>
struct std::hash<Flex::Entity>
{
	size_t operator()(const Flex::Entity& entity) const noexcept
	{
		return std::hash<uint64_t>{}((static_cast<uint64_t>(entity.Generation) << 32) | entity.Index);
	}
};
