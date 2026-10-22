#pragma once
#include "Core/Math/Rect.h"
#include "Core/Math/Vector2.h"
#include <algorithm>
#include <cmath>

namespace Flex
{
	// Small, header-only 2D collision helpers (inlined into hot loops, like Core/Math).
	// A full physics system with bodies and colliders is a later step.
	namespace Collision2D
	{
		// Keeps a moving circle inside bounds: pushes it back in and reflects its velocity
		// off every wall it touches. restitution 1 = perfectly bouncy. Returns true on a bounce.
		inline bool BounceCircleInsideRect(Vector2& center, Vector2& velocity, float radius, const Rect& bounds, float restitution = 1.0f)
		{
			bool bBounced = false;
			const float minX = bounds.x + radius;
			const float maxX = bounds.x + bounds.width - radius;
			const float minY = bounds.y + radius;
			const float maxY = bounds.y + bounds.height - radius;
			if (center.x < minX) {
				center.x = minX;
				velocity.x = std::abs(velocity.x) * restitution;
				bBounced = true;
			} else if (center.x > maxX) {
				center.x = maxX;
				velocity.x = -std::abs(velocity.x) * restitution;
				bBounced = true;
			}
			if (center.y < minY) {
				center.y = minY;
				velocity.y = std::abs(velocity.y) * restitution;
				bBounced = true;
			} else if (center.y > maxY) {
				center.y = maxY;
				velocity.y = -std::abs(velocity.y) * restitution;
				bBounced = true;
			}
			return bBounced;
		}

		// Pushes a circle out of a solid box and reflects its velocity relative to the box,
		// so a moving box shoves circles along. Returns true on contact.
		inline bool BounceCircleOffRect(Vector2& center, Vector2& velocity, float radius, const Rect& box, const Vector2& boxVelocity = {}, float restitution = 1.0f)
		{
			const float right = box.x + box.width;
			const float bottom = box.y + box.height;
			// Cheap reject first: most circles are nowhere near the box.
			if (center.x + radius <= box.x || center.x - radius >= right || center.y + radius <= box.y || center.y - radius >= bottom) {
				return false;
			}

			const Vector2 closest{ std::clamp(center.x, box.x, right), std::clamp(center.y, box.y, bottom) };
			const Vector2 delta = center - closest;
			const float distanceSquared = delta.LengthSquared();
			Vector2 normal;
			float penetration;
			if (distanceSquared > 0.0f) {
				if (distanceSquared >= radius * radius) {
					return false; // near a corner but not touching
				}
				const float distance = std::sqrt(distanceSquared);
				normal = delta / distance;
				penetration = radius - distance;
			} else {
				// Center is inside the box: leave through the nearest side.
				const float toLeft = center.x - box.x;
				const float toRight = right - center.x;
				const float toTop = center.y - box.y;
				const float toBottom = bottom - center.y;
				const float nearest = std::min({ toLeft, toRight, toTop, toBottom });
				if (nearest == toLeft) {
					normal = { -1.0f, 0.0f };
				} else if (nearest == toRight) {
					normal = { 1.0f, 0.0f };
				} else if (nearest == toTop) {
					normal = { 0.0f, -1.0f };
				} else {
					normal = { 0.0f, 1.0f };
				}
				penetration = nearest + radius;
			}

			center += normal * penetration;
			const float approachSpeed = Vector2::Dot(velocity - boxVelocity, normal);
			if (approachSpeed < 0.0f) {
				velocity -= normal * ((1.0f + restitution) * approachSpeed);
			}
			return true;
		}

		// Moves rect so it lies inside bounds (a rect bigger than bounds sticks to the top-left).
		inline Rect ClampRectInside(const Rect& rect, const Rect& bounds)
		{
			Rect result = rect;
			result.x = std::max(bounds.x, std::min(rect.x, bounds.x + bounds.width - rect.width));
			result.y = std::max(bounds.y, std::min(rect.y, bounds.y + bounds.height - rect.height));
			return result;
		}
	}
}
