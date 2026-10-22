#pragma once
#include "Core/Math/Vector2.h"
#include <algorithm>

namespace Flex
{
	// Axis-aligned rectangle in world/screen units: culling, selection, broadphase collision.
	// (x, y) is the top-left corner (y points down). Half-open: contains [x, x + width) x [y, y + height).
	struct Rect
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;

		constexpr Rect() = default;
		constexpr Rect(float x, float y, float width, float height) : x{ x }, y{ y }, width{ width }, height{ height } {}

		static constexpr Rect FromMinMax(const Vector2& min, const Vector2& max)
		{
			return { min.x, min.y, max.x - min.x, max.y - min.y };
		}

		static constexpr Rect FromCenter(const Vector2& center, const Vector2& size)
		{
			return { center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y };
		}

		constexpr Vector2 GetMin() const { return { x, y }; }
		constexpr Vector2 GetMax() const { return { x + width, y + height }; }
		constexpr Vector2 GetSize() const { return { width, height }; }
		constexpr Vector2 GetCenter() const { return { x + width * 0.5f, y + height * 0.5f }; }
		constexpr bool IsEmpty() const { return width <= 0.0f || height <= 0.0f; }

		constexpr bool Contains(const Vector2& point) const
		{
			return point.x >= x && point.x < x + width && point.y >= y && point.y < y + height;
		}

		// Touching edges don't count as overlapping.
		constexpr bool Overlaps(const Rect& other) const
		{
			return x < other.x + other.width && other.x < x + width && y < other.y + other.height && other.y < y + height;
		}

		// Empty rect (width/height 0) when they don't overlap.
		Rect Intersection(const Rect& other) const
		{
			const float left = std::max(x, other.x);
			const float top = std::max(y, other.y);
			const float right = std::min(x + width, other.x + other.width);
			const float bottom = std::min(y + height, other.y + other.height);
			if (right <= left || bottom <= top) {
				return {};
			}
			return { left, top, right - left, bottom - top };
		}

		// Smallest rect that contains both.
		Rect Union(const Rect& other) const
		{
			const float left = std::min(x, other.x);
			const float top = std::min(y, other.y);
			return { left, top, std::max(x + width, other.x + other.width) - left, std::max(y + height, other.y + other.height) - top };
		}

		constexpr Rect Expanded(float amount) const
		{
			return { x - amount, y - amount, width + amount * 2.0f, height + amount * 2.0f };
		}

		constexpr bool operator==(const Rect&) const = default;
	};
}
