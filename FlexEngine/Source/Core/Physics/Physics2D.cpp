#include "Core/Physics/Physics2D.h"
#include "Core/Math/Rect.h"
#include "Core/Physics/Collision2D.h"
#include "Core/World/Transform.h"
#include "Core/World/World.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Flex
{
	FLEX_REGISTER_COMPONENT(RigidBody2D)
	FLEX_REGISTER_COMPONENT(BoxCollider2D)
	FLEX_REGISTER_COMPONENT(CircleCollider2D)

	const char* ToString(BodyType type)
	{
		switch (type) {
		case BodyType::Static:
			return "Static";
		case BodyType::Kinematic:
			return "Kinematic";
		case BodyType::Dynamic:
			return "Dynamic";
		}
		return "Dynamic";
	}

	bool TryParseBodyType(const std::string& text, BodyType& outType)
	{
		for (BodyType type : { BodyType::Static, BodyType::Kinematic, BodyType::Dynamic }) {
			if (text == ToString(type)) {
				outType = type;
				return true;
			}
		}
		return false;
	}

	namespace
	{
		struct StaticCircle
		{
			Vector2 Center;
			float Radius = 0.0f;
		};

		bool IsStatic(const RigidBody2D* body)
		{
			return !body || body->Type != BodyType::Dynamic;
		}

		Vector2 AbsScale(const WorldTransform& worldTransform)
		{
			const Vector2 scale = worldTransform.GetScale();
			return { std::abs(scale.x), std::abs(scale.y) };
		}

		// Pushes a box out of another along the shallower axis and reflects that velocity component.
		bool BounceBoxOffBox(Rect& box, Vector2& velocity, const Rect& other, float restitution)
		{
			if (!box.Overlaps(other)) {
				return false;
			}
			const float pushLeft = (box.x + box.width) - other.x;
			const float pushRight = (other.x + other.width) - box.x;
			const float pushUp = (box.y + box.height) - other.y;
			const float pushDown = (other.y + other.height) - box.y;
			const float pushX = pushLeft < pushRight ? -pushLeft : pushRight;
			const float pushY = pushUp < pushDown ? -pushUp : pushDown;
			if (std::abs(pushX) < std::abs(pushY)) {
				box.x += pushX;
				if ((pushX < 0.0f && velocity.x > 0.0f) || (pushX > 0.0f && velocity.x < 0.0f)) {
					velocity.x = -velocity.x * restitution;
				}
			} else {
				box.y += pushY;
				if ((pushY < 0.0f && velocity.y > 0.0f) || (pushY > 0.0f && velocity.y < 0.0f)) {
					velocity.y = -velocity.y * restitution;
				}
			}
			return true;
		}
	}

	PhysicsStats StepPhysics(World& world, float deltaTime, const PhysicsSettings& settings)
	{
		PhysicsStats stats;

		// Static colliders this step, in world space.
		std::vector<Rect> staticBoxes;
		std::vector<StaticCircle> staticCircles;
		world.Each<const WorldTransform, const BoxCollider2D>([&](Entity entity, const WorldTransform& worldTransform, const BoxCollider2D& box) {
			if (IsStatic(world.GetComponent<RigidBody2D>(entity))) {
				const Vector2 scale = AbsScale(worldTransform);
				const Vector2 center = worldTransform.Matrix.TransformPoint(box.Offset);
				staticBoxes.push_back(Rect::FromCenter(center, box.Size * scale));
			}
		});
		world.Each<const WorldTransform, const CircleCollider2D>([&](Entity entity, const WorldTransform& worldTransform, const CircleCollider2D& circle) {
			if (IsStatic(world.GetComponent<RigidBody2D>(entity))) {
				const Vector2 scale = AbsScale(worldTransform);
				staticCircles.push_back({ worldTransform.Matrix.TransformPoint(circle.Offset), circle.Radius * std::max(scale.x, scale.y) });
			}
		});
		stats.StaticColliders = static_cast<uint32_t>(staticBoxes.size() + staticCircles.size());

		// Velocity: gravity, drag; kinematic bodies just move.
		world.Each<Transform, RigidBody2D>([&](Transform& transform, RigidBody2D& body) {
			if (body.Type == BodyType::Dynamic) {
				body.Velocity += settings.Gravity * (body.GravityScale * deltaTime);
				if (body.LinearDrag > 0.0f) {
					body.Velocity *= std::max(0.0f, 1.0f - body.LinearDrag * deltaTime);
				}
				transform.Position += body.Velocity * deltaTime;
				++stats.DynamicBodies;
			} else if (body.Type == BodyType::Kinematic) {
				transform.Position += body.Velocity * deltaTime;
			}
		});

		if (staticBoxes.empty() && staticCircles.empty()) {
			return stats;
		}

		// Dynamic circles against the static colliders.
		world.Each<Transform, RigidBody2D, const CircleCollider2D>([&](Transform& transform, RigidBody2D& body, const CircleCollider2D& circle) {
			if (body.Type != BodyType::Dynamic) {
				return;
			}
			const Vector2 scale{ std::abs(transform.Scale.x), std::abs(transform.Scale.y) };
			const Vector2 offset = circle.Offset * scale;
			const float radius = circle.Radius * std::max(scale.x, scale.y);
			Vector2 center = transform.Position + offset;
			for (const Rect& box : staticBoxes) {
				if (Collision2D::BounceCircleOffRect(center, body.Velocity, radius, box, {}, body.Restitution)) {
					++stats.Contacts;
				}
			}
			for (const StaticCircle& other : staticCircles) {
				const Vector2 delta = center - other.Center;
				const float minDistance = radius + other.Radius;
				const float distanceSquared = delta.LengthSquared();
				if (distanceSquared >= minDistance * minDistance || distanceSquared <= 0.0f) {
					continue;
				}
				const float distance = std::sqrt(distanceSquared);
				const Vector2 normal = delta / distance;
				center += normal * (minDistance - distance);
				const float approachSpeed = Vector2::Dot(body.Velocity, normal);
				if (approachSpeed < 0.0f) {
					body.Velocity -= normal * ((1.0f + body.Restitution) * approachSpeed);
				}
				++stats.Contacts;
			}
			transform.Position = center - offset;
		});

		// Dynamic boxes against the static boxes.
		world.Each<Transform, RigidBody2D, const BoxCollider2D>([&](Transform& transform, RigidBody2D& body, const BoxCollider2D& collider) {
			if (body.Type != BodyType::Dynamic) {
				return;
			}
			const Vector2 scale{ std::abs(transform.Scale.x), std::abs(transform.Scale.y) };
			const Vector2 offset = collider.Offset * scale;
			Rect box = Rect::FromCenter(transform.Position + offset, collider.Size * scale);
			for (const Rect& other : staticBoxes) {
				if (BounceBoxOffBox(box, body.Velocity, other, body.Restitution)) {
					++stats.Contacts;
				}
			}
			transform.Position = box.GetCenter() - offset;
		});

		return stats;
	}
}
