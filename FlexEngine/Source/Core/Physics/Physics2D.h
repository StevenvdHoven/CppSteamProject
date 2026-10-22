#pragma once
#include "Core/Core.h"
#include "Core/Math/Vector2.h"
#include "Core/Serialization/Archive.h"
#include "Core/World/ComponentRegistery.h"
#include <cstdint>
#include <string>

namespace Flex
{
	class World;

	enum class BodyType : uint8_t
	{
		Static,    // never moves; other bodies bounce off it (same as a collider without a body)
		Kinematic, // moves by its Velocity only, ignores collisions; others bounce off it
		Dynamic    // moved by velocity, gravity and collisions
	};

	FLEX_API const char* ToString(BodyType type);
	FLEX_API bool TryParseBodyType(const std::string& text, BodyType& outType);

	// Makes an entity move physically. Moves the entity's local Transform, so bodies should be
	// roots (no parent). Needs a BoxCollider2D or CircleCollider2D to collide.
	struct RigidBody2D
	{
		FLEX_COMPONENT(RigidBody2D)
		static constexpr uint32_t Version = 1;

		BodyType Type = BodyType::Dynamic;
		Vector2 Velocity;           // units per second
		float GravityScale = 1.0f;  // times PhysicsSettings::Gravity
		float Restitution = 1.0f;   // bounciness: 1 keeps all speed, 0 stops dead
		float LinearDrag = 0.0f;    // fraction of the speed lost per second

		void Serialize(Archive& archive)
		{
			archive.SerializeVersion(Version);
			// Stored as text ("Dynamic") so scene files stay readable.
			std::string type = ToString(Type);
			archive.Field("Type", type);
			if (archive.IsLoading()) {
				TryParseBodyType(type, Type);
			}
			archive.Field("Velocity", Velocity);
			archive.Field("GravityScale", GravityScale);
			archive.Field("Restitution", Restitution);
			archive.Field("LinearDrag", LinearDrag);
		}
	};

	// Axis-aligned box in the entity's space (rotation is ignored; size is scaled by the transform).
	struct BoxCollider2D
	{
		FLEX_COMPONENT(BoxCollider2D)
		static constexpr uint32_t Version = 1;

		Vector2 Size{ 32.0f, 32.0f };
		Vector2 Offset;

		void Serialize(Archive& archive)
		{
			archive.SerializeVersion(Version);
			archive.Field("Size", Size);
			archive.Field("Offset", Offset);
		}
	};

	// Circle around the entity (radius scaled by the larger transform scale).
	struct CircleCollider2D
	{
		FLEX_COMPONENT(CircleCollider2D)
		static constexpr uint32_t Version = 1;

		float Radius = 16.0f;
		Vector2 Offset;

		void Serialize(Archive& archive)
		{
			archive.SerializeVersion(Version);
			archive.Field("Radius", Radius);
			archive.Field("Offset", Offset);
		}
	};

	struct PhysicsSettings
	{
		Vector2 Gravity{ 0.0f, 980.0f }; // units per second squared (y down)
		float FixedTimeStep = 1.0f / 60.0f;
	};

	struct PhysicsStats
	{
		uint32_t DynamicBodies = 0;
		uint32_t StaticColliders = 0;
		uint32_t Contacts = 0;
	};

	// One fixed physics step:
	//  - kinematic bodies move by their velocity
	//  - dynamic bodies get gravity and drag, move, and bounce off static colliders
	//    (colliders without a body, or with a Static/Kinematic body)
	// Dynamic bodies don't collide with each other yet. Call UpdateWorldTransforms afterwards.
	FLEX_API PhysicsStats StepPhysics(World& world, float deltaTime, const PhysicsSettings& settings);
}
