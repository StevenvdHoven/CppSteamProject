#pragma once
#include "Core/Core.h"
#include "Core/Math/Matrix2x3.h"
#include "Core/Math/Vector2.h"
#include "Core/Serialization/Archive.h"
#include "Core/World/ComponentRegistery.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

namespace Flex
{
	class World;

	enum class BodyType : uint8_t
	{
		Static,    // never moves; same as a collider without a body
		Kinematic, // moves by its velocities only, pushes dynamic bodies but isn't pushed back
		Dynamic    // moved by gravity, velocities and collisions
	};

	FLEX_API const char* ToString(BodyType type);
	FLEX_API bool TryParseBodyType(const std::string& text, BodyType& outType);

	// Makes an entity move physically. Moves the entity's local Transform, so bodies should be
	// roots (no parent). Needs a BoxCollider2D or CircleCollider2D to collide (one per body; with
	// both, the box is used). The body turns around the centre of its collider.
	//
	// Friction and Restitution belong to the body; a collider without a body uses
	// DefaultFriction and DefaultRestitution. Two bodies touching use sqrt(frictionA * frictionB)
	// and max(restitutionA, restitutionB).
	struct RigidBody2D
	{
		FLEX_COMPONENT(RigidBody2D)
		static constexpr uint32_t Version = 2;
		static constexpr float DefaultFriction = 0.5f;
		static constexpr float DefaultRestitution = 0.0f;

		BodyType Type = BodyType::Dynamic;
		bool bFixedRotation = false;    // never turns
		bool bCanSleep = true;          // may stop being simulated while it lies still
		bool bIsSleeping = false;       // runtime state (see below)
		float Mass = 1.0f;              // any unit: only the ratio between bodies matters
		Vector2 Velocity;               // units per second
		float AngularVelocity = 0.0f;   // radians per second, positive = clockwise
		float GravityScale = 1.0f;      // times PhysicsSettings::Gravity
		float LinearDrag = 0.0f;        // fraction of the speed lost per second
		float AngularDrag = 0.0f;       // fraction of the spin lost per second
		float Friction = DefaultFriction;
		float Restitution = DefaultRestitution; // bounciness: 0 stops dead, 1 keeps all speed

		// bIsSleeping and SleepTimer are runtime state, written by PhysicsWorld2D (not saved).
		// Setting Velocity or AngularVelocity on a sleeping body wakes it up.
		float SleepTimer = 0.0f;

		void Serialize(Archive& archive)
		{
			const uint32_t version = archive.SerializeVersion(Version);
			// Stored as text ("Dynamic") so scene files stay readable.
			std::string type = ToString(Type);
			archive.Field("Type", type);
			if (archive.IsLoading()) {
				TryParseBodyType(type, Type);
			}
			if (version >= 2) {
				archive.Field("Mass", Mass);
			}
			archive.Field("Velocity", Velocity);
			if (version >= 2) {
				archive.Field("AngularVelocity", AngularVelocity);
			}
			archive.Field("GravityScale", GravityScale);
			archive.Field("LinearDrag", LinearDrag);
			if (version >= 2) {
				archive.Field("AngularDrag", AngularDrag);
				archive.Field("Friction", Friction);
			}
			archive.Field("Restitution", Restitution);
			if (version >= 2) {
				archive.Field("FixedRotation", bFixedRotation);
				archive.Field("CanSleep", bCanSleep);
			}
		}
	};

	// Two colliders touch when (layerA & maskB) and (layerB & maskA) are both non-zero.
	constexpr uint32_t DefaultCollisionLayer = 1u;
	constexpr uint32_t AllCollisionLayers = 0xFFFFFFFFu;

	// Box in the entity's space: it turns with the transform's rotation, and its size is
	// scaled by the transform's scale.
	struct BoxCollider2D
	{
		FLEX_COMPONENT(BoxCollider2D)
		static constexpr uint32_t Version = 2;

		Vector2 Size{ 32.0f, 32.0f };
		Vector2 Offset;
		uint32_t Layer = DefaultCollisionLayer;
		uint32_t Mask = AllCollisionLayers;

		void Serialize(Archive& archive)
		{
			const uint32_t version = archive.SerializeVersion(Version);
			archive.Field("Size", Size);
			archive.Field("Offset", Offset);
			if (version >= 2) {
				archive.Field("Layer", Layer);
				archive.Field("Mask", Mask);
			}
		}
	};

	// Circle around the entity (radius scaled by the larger transform scale).
	struct CircleCollider2D
	{
		FLEX_COMPONENT(CircleCollider2D)
		static constexpr uint32_t Version = 2;

		float Radius = 16.0f;
		Vector2 Offset;
		uint32_t Layer = DefaultCollisionLayer;
		uint32_t Mask = AllCollisionLayers;

		void Serialize(Archive& archive)
		{
			const uint32_t version = archive.SerializeVersion(Version);
			archive.Field("Radius", Radius);
			archive.Field("Offset", Offset);
			if (version >= 2) {
				archive.Field("Layer", Layer);
				archive.Field("Mask", Mask);
			}
		}
	};

	// A collider's shape in world space, exactly as the physics sees it (also used to draw gizmos).
	struct ColliderShape2D
	{
		bool bIsCircle = false;
		Vector2 Center;
		float Rotation = 0.0f;
		Vector2 HalfSize; // box
		float Radius = 0.0f; // circle

		// Box corners, clockwise on screen starting top-left (before rotation).
		void GetCorners(Vector2 outCorners[4]) const
		{
			const Vector2 axisX = Vector2::FromAngle(Rotation) * HalfSize.x;
			const Vector2 axisY = Vector2::FromAngle(Rotation).Perpendicular() * HalfSize.y;
			outCorners[0] = Center - axisX - axisY;
			outCorners[1] = Center + axisX - axisY;
			outCorners[2] = Center + axisX + axisY;
			outCorners[3] = Center - axisX + axisY;
		}

		// From a world matrix (rotation and scale only, skew is ignored).
		static ColliderShape2D FromBox(const Matrix2x3& world, const BoxCollider2D& box)
		{
			const Vector2 scale = world.GetScale();
			ColliderShape2D shape;
			shape.Center = world.TransformPoint(box.Offset);
			shape.Rotation = world.GetRotation();
			shape.HalfSize = { std::abs(box.Size.x * scale.x) * 0.5f, std::abs(box.Size.y * scale.y) * 0.5f };
			return shape;
		}

		static ColliderShape2D FromCircle(const Matrix2x3& world, const CircleCollider2D& circle)
		{
			const Vector2 scale = world.GetScale();
			ColliderShape2D shape;
			shape.bIsCircle = true;
			shape.Center = world.TransformPoint(circle.Offset);
			shape.Rotation = world.GetRotation();
			shape.Radius = circle.Radius * std::max(std::abs(scale.x), std::abs(scale.y));
			return shape;
		}
	};

	// Project-wide physics settings (the [Physics] section of the .flexproj, edited in the
	// editor's Project window). Units are world units (pixels at zoom 1) and seconds.
	struct PhysicsSettings
	{
		static constexpr uint32_t Version = 1;

		Vector2 Gravity{ 0.0f, 980.0f }; // units per second squared (y down)
		float FixedTimeStep = 1.0f / 60.0f;
		int32_t Substeps = 8;            // contact solving per fixed step; more = stiffer stacks
		int32_t VelocityIterations = 2;  // per sub-step
		int32_t PositionIterations = 3;  // more = less sinking into each other
		float RestitutionThreshold = 60.0f; // slower impacts (units/s) don't bounce, so stacks come to rest
		float LinearSlop = 0.5f;         // allowed overlap (units) that keeps resting contacts steady
		bool bAllowSleep = true;
		float TimeToSleep = 0.5f;        // seconds a body must lie still before it sleeps
		float SleepLinearSpeed = 4.0f;   // units per second
		float SleepAngularSpeed = 0.05f; // radians per second

		void Serialize(Archive& archive)
		{
			archive.SerializeVersion(Version);
			archive.Field("Gravity", Gravity);
			archive.Field("FixedTimeStep", FixedTimeStep);
			archive.Field("Substeps", Substeps);
			archive.Field("VelocityIterations", VelocityIterations);
			archive.Field("PositionIterations", PositionIterations);
			archive.Field("RestitutionThreshold", RestitutionThreshold);
			archive.Field("LinearSlop", LinearSlop);
			archive.Field("AllowSleep", bAllowSleep);
			archive.Field("TimeToSleep", TimeToSleep);
			archive.Field("SleepLinearSpeed", SleepLinearSpeed);
			archive.Field("SleepAngularSpeed", SleepAngularSpeed);
		}
	};

	struct PhysicsStats
	{
		uint32_t DynamicBodies = 0;
		uint32_t SleepingBodies = 0;
		uint32_t StaticColliders = 0;
		uint32_t Contacts = 0; // contact points
	};

	// The rigid-body simulation of one World. It keeps the contacts of the previous step
	// (warm starting), so use one PhysicsWorld2D per running world and Reset() it when the
	// world is replaced.
	//
	// Each Step: find touching pairs (uniform grid; layers/masks filter them), build contact
	// manifolds (oriented boxes and circles, up to 2 points, kept up to LinearSlop apart), then
	// in Substeps sub-steps: gravity and drag, friction and contact impulses (sequential impulses,
	// warm started, both points of a face contact solved together, highest contacts first) and
	// moving the bodies; finally push overlaps apart and let islands of resting bodies sleep.
	// Bodies that can't touch anything this step are simply moved (cheap for big swarms).
	// Call UpdateWorldTransforms afterwards.
	class FLEX_API PhysicsWorld2D
	{
	public:
		PhysicsWorld2D();
		~PhysicsWorld2D();
		PhysicsWorld2D(PhysicsWorld2D&&) noexcept;
		PhysicsWorld2D& operator=(PhysicsWorld2D&&) noexcept;

		PhysicsStats Step(World& world, float deltaTime, const PhysicsSettings& settings);
		void Reset();

	private:
		struct State;
		std::unique_ptr<State> Data;
	};
}
