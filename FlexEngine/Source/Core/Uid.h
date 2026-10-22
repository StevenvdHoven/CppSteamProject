#pragma once
#include "Core/Core.h"
#include <compare>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Flex
{
	// Random 64-bit identifier for assets (and later scene objects).
	// 0 means "no id". Written as 16 lowercase hex characters, e.g. a41f09c3d2e85b70.
	class FLEX_API Uid
	{
	public:
		constexpr Uid() = default;
		constexpr explicit Uid(uint64_t value) : Value{ value } {}

		static Uid Generate();

		// Accepts exactly 16 hex characters. Returns nothing if the text isn't a valid id.
		static std::optional<Uid> FromString(std::string_view text);
		std::string ToString() const;

		constexpr bool IsValid() const { return Value != 0; }
		constexpr uint64_t GetValue() const { return Value; }

		constexpr auto operator<=>(const Uid&) const = default;

	private:
		uint64_t Value = 0;
	};
}

template<>
struct std::hash<Flex::Uid>
{
	size_t operator()(const Flex::Uid& uid) const noexcept
	{
		return std::hash<uint64_t>{}(uid.GetValue());
	}
};
