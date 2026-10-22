#pragma once
#include "Core/Math/MathUtils.h"
#include <cmath>
#include <cstdint>
#include <functional>

namespace Flex
{
	// 2D vector of floats. Positions, velocities and directions (y points down, see MathUtils.h).
	// Equality is EXACT; use NearlyEqual for tolerant comparison.
	struct Vector2
	{
		float x = 0.0f;
		float y = 0.0f;

		constexpr Vector2() = default;
		constexpr Vector2(float x, float y) : x{ x }, y{ y } {}

		static constexpr Vector2 Zero() { return { 0.0f, 0.0f }; }
		static constexpr Vector2 One() { return { 1.0f, 1.0f }; }
		static constexpr Vector2 Right() { return { 1.0f, 0.0f }; }
		static constexpr Vector2 Down() { return { 0.0f, 1.0f }; }

		// Unit vector pointing at an angle (0 = right, positive = clockwise on screen).
		static Vector2 FromAngle(float radians) { return { std::cos(radians), std::sin(radians) }; }

		constexpr Vector2 operator+(const Vector2& other) const { return { x + other.x, y + other.y }; }
		constexpr Vector2 operator-(const Vector2& other) const { return { x - other.x, y - other.y }; }
		constexpr Vector2 operator*(const Vector2& other) const { return { x * other.x, y * other.y }; }
		constexpr Vector2 operator/(const Vector2& other) const { return { x / other.x, y / other.y }; }
		constexpr Vector2 operator*(float scalar) const { return { x * scalar, y * scalar }; }
		constexpr Vector2 operator/(float scalar) const { return { x / scalar, y / scalar }; }
		constexpr Vector2 operator-() const { return { -x, -y }; }

		constexpr Vector2& operator+=(const Vector2& other) { x += other.x; y += other.y; return *this; }
		constexpr Vector2& operator-=(const Vector2& other) { x -= other.x; y -= other.y; return *this; }
		constexpr Vector2& operator*=(const Vector2& other) { x *= other.x; y *= other.y; return *this; }
		constexpr Vector2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
		constexpr Vector2& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }

		constexpr bool operator==(const Vector2&) const = default;

		constexpr float LengthSquared() const { return x * x + y * y; }
		float Length() const { return std::sqrt(LengthSquared()); }

		// Zero vector stays zero (no division by zero).
		Vector2 Normalized() const
		{
			const float length = Length();
			return length > Math::Epsilon ? Vector2{ x / length, y / length } : Vector2{};
		}

		// Angle of this vector (0 = right, positive = clockwise on screen).
		float Angle() const { return std::atan2(y, x); }

		// Rotated by an angle (positive = clockwise on screen).
		Vector2 Rotated(float radians) const
		{
			const float cosine = std::cos(radians);
			const float sine = std::sin(radians);
			return { x * cosine - y * sine, x * sine + y * cosine };
		}

		// 90 degrees clockwise on screen.
		constexpr Vector2 Perpendicular() const { return { -y, x }; }

		static constexpr float Dot(const Vector2& a, const Vector2& b) { return a.x * b.x + a.y * b.y; }

		// z of the 3D cross product: > 0 when b is clockwise from a (on screen).
		static constexpr float Cross(const Vector2& a, const Vector2& b) { return a.x * b.y - a.y * b.x; }

		static constexpr float DistanceSquared(const Vector2& a, const Vector2& b) { return (b - a).LengthSquared(); }
		static float Distance(const Vector2& a, const Vector2& b) { return (b - a).Length(); }

		static constexpr Vector2 Lerp(const Vector2& from, const Vector2& to, float t) { return from + (to - from) * t; }

		static Vector2 MoveTowards(const Vector2& current, const Vector2& target, float maxDistance)
		{
			const Vector2 delta = target - current;
			const float distance = delta.Length();
			if (distance <= maxDistance || distance <= Math::Epsilon) {
				return target;
			}
			return current + delta / distance * maxDistance;
		}

		// Signed angle from a to b, in (-Pi, Pi].
		static float AngleBetween(const Vector2& a, const Vector2& b) { return std::atan2(Cross(a, b), Dot(a, b)); }

		static bool NearlyEqual(const Vector2& a, const Vector2& b, float tolerance = 1e-5f)
		{
			return Math::NearlyEqual(a.x, b.x, tolerance) && Math::NearlyEqual(a.y, b.y, tolerance);
		}
	};

	constexpr Vector2 operator*(float scalar, const Vector2& vector) { return vector * scalar; }

	// Whole-number 2D vector: grid cells (RTS maps, tilemaps), pixel coordinates.
	struct Vector2Int
	{
		int32_t x = 0;
		int32_t y = 0;

		constexpr Vector2Int() = default;
		constexpr Vector2Int(int32_t x, int32_t y) : x{ x }, y{ y } {}

		constexpr Vector2Int operator+(const Vector2Int& other) const { return { x + other.x, y + other.y }; }
		constexpr Vector2Int operator-(const Vector2Int& other) const { return { x - other.x, y - other.y }; }
		constexpr Vector2Int operator*(int32_t scalar) const { return { x * scalar, y * scalar }; }
		constexpr Vector2Int& operator+=(const Vector2Int& other) { x += other.x; y += other.y; return *this; }
		constexpr Vector2Int& operator-=(const Vector2Int& other) { x -= other.x; y -= other.y; return *this; }
		constexpr bool operator==(const Vector2Int&) const = default;

		constexpr Vector2 ToVector2() const { return { static_cast<float>(x), static_cast<float>(y) }; }

		// Rounds down, so negative coordinates map to the correct grid cell (-0.5 -> -1).
		static Vector2Int FloorToInt(const Vector2& vector)
		{
			return { static_cast<int32_t>(std::floor(vector.x)), static_cast<int32_t>(std::floor(vector.y)) };
		}
	};
}

template<>
struct std::hash<Flex::Vector2Int>
{
	size_t operator()(const Flex::Vector2Int& vector) const noexcept
	{
		return std::hash<uint64_t>{}((static_cast<uint64_t>(static_cast<uint32_t>(vector.x)) << 32) | static_cast<uint32_t>(vector.y));
	}
};
