#pragma once
#include "Core/Math/Vector2.h"
#include <cmath>

namespace Flex
{
	// 2D affine transform (rotation, scale, translation), the same layout as in SpaceAdventure:
	//
	//     | dirX.x  dirY.x  orig.x |
	//     | dirX.y  dirY.y  orig.y |
	//
	// dirX/dirY are where the local x/y axes end up, orig is where the local origin ends up.
	// A * B means "apply B first, then A", so world = parentWorld * local.
	struct Matrix2x3
	{
		Vector2 dirX{ 1.0f, 0.0f };
		Vector2 dirY{ 0.0f, 1.0f };
		Vector2 orig{ 0.0f, 0.0f };

		static constexpr Matrix2x3 Identity() { return {}; }

		static constexpr Matrix2x3 FromTranslation(const Vector2& translation)
		{
			return { { 1.0f, 0.0f }, { 0.0f, 1.0f }, translation };
		}

		// Positive = clockwise on screen (y points down).
		static Matrix2x3 FromRotation(float radians)
		{
			const float cosine = std::cos(radians);
			const float sine = std::sin(radians);
			return { { cosine, sine }, { -sine, cosine }, {} };
		}

		static constexpr Matrix2x3 FromScale(const Vector2& scale)
		{
			return { { scale.x, 0.0f }, { 0.0f, scale.y }, {} };
		}

		// Scale, then rotate, then translate: the usual object/sprite transform.
		// Same result as FromTranslation(t) * FromRotation(r) * FromScale(s), but without the multiplications.
		static Matrix2x3 FromTRS(const Vector2& translation, float radians, const Vector2& scale)
		{
			const float cosine = std::cos(radians);
			const float sine = std::sin(radians);
			return { { cosine * scale.x, sine * scale.x }, { -sine * scale.y, cosine * scale.y }, translation };
		}

		constexpr Matrix2x3 operator*(const Matrix2x3& rhs) const
		{
			return {
				TransformDirection(rhs.dirX),
				TransformDirection(rhs.dirY),
				TransformPoint(rhs.orig)
			};
		}

		constexpr Vector2 TransformPoint(const Vector2& point) const
		{
			return dirX * point.x + dirY * point.y + orig;
		}

		// Ignores translation (for velocities, directions, sizes).
		constexpr Vector2 TransformDirection(const Vector2& direction) const
		{
			return dirX * direction.x + dirY * direction.y;
		}

		constexpr float Determinant() const
		{
			return dirX.x * dirY.y - dirY.x * dirX.y;
		}

		// A matrix that can't be inverted (zero scale) gives Identity.
		Matrix2x3 Inverse() const
		{
			const float determinant = Determinant();
			if (!(std::fabs(determinant) > Math::Epsilon * Math::Epsilon)) { // also catches NaN
				return Identity();
			}

			// Written as a multiplication by a reciprocal computed from a value known to be non-zero,
			// so the optimiser doesn't report a (false) "divide by 0" warning (C4723) in game code.
			const float inverse = 1.0f / (determinant != 0.0f ? determinant : 1.0f);
			Matrix2x3 result;
			result.dirX = Vector2{ dirY.y, -dirX.y } * inverse;
			result.dirY = Vector2{ -dirY.x, dirX.x } * inverse;
			result.orig = -(result.dirX * orig.x + result.dirY * orig.y);
			return result;
		}

		// Decomposition (assumes no skew, which holds for anything built from TRS).
		constexpr const Vector2& GetTranslation() const { return orig; }
		float GetRotation() const { return std::atan2(dirX.y, dirX.x); }
		Vector2 GetScale() const
		{
			// A mirrored transform has a negative determinant; report that as a negative y scale.
			const float signY = Determinant() < 0.0f ? -1.0f : 1.0f;
			return { dirX.Length(), dirY.Length() * signY };
		}

		constexpr bool operator==(const Matrix2x3&) const = default;
	};
}
