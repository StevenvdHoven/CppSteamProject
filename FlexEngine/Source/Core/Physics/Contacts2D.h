#pragma once
#include "Core/Math/Vector2.h"
#include "Core/Physics/Physics2D.h"
#include "Core/World/Entity.h"
#include "Core/World/Transform.h"
#include <cmath>
#include <cstdint>

// Internal to PhysicsWorld2D: the bodies and contact manifolds the solver works on.
namespace Flex::Physics
{
	struct Rotation2
	{
		float Cos = 1.0f;
		float Sin = 0.0f;

		static Rotation2 FromAngle(float radians) { return { std::cos(radians), std::sin(radians) }; }

		Vector2 Rotate(const Vector2& v) const { return { Cos * v.x - Sin * v.y, Sin * v.x + Cos * v.y }; }
		Vector2 InverseRotate(const Vector2& v) const { return { Cos * v.x + Sin * v.y, -Sin * v.x + Cos * v.y }; }
	};

	// cross(w, r) for a spin w: the velocity of point r on a body turning at w.
	inline Vector2 CrossSV(float w, const Vector2& r) { return { -w * r.y, w * r.x }; }

	struct SolverBody
	{
		Entity Owner;
		Transform* TransformData = nullptr; // written back (dynamic and kinematic bodies)
		RigidBody2D* Body = nullptr;        // nullptr for a collider without a body

		BodyType Type = BodyType::Static;
		bool bHasShape = false;
		bool bIsCircle = false;
		bool bSleeping = false;

		Vector2 Center;      // centre of mass (= collider centre) in world space
		float Angle = 0.0f;
		Rotation2 Rot;
		Vector2 LocalCenter; // centre of mass relative to the transform position, unrotated
		Vector2 HalfSize;
		float Radius = 0.0f;

		Vector2 Velocity;
		float AngularVelocity = 0.0f;
		float InvMass = 0.0f;
		float InvInertia = 0.0f;
		float Friction = RigidBody2D::DefaultFriction;
		float Restitution = RigidBody2D::DefaultRestitution;
		uint32_t Layer = DefaultCollisionLayer;
		uint32_t Mask = AllCollisionLayers;

		Vector2 Min; // bounding box
		Vector2 Max;

		bool IsAwakeDynamic() const { return Type == BodyType::Dynamic && !bSleeping; }
		bool IsMovingKinematic() const { return Type == BodyType::Kinematic && (Velocity.LengthSquared() > 0.0f || AngularVelocity != 0.0f); }
	};

	enum class ManifoldKind : uint8_t
	{
		BoxBox,       // A's face is the reference face
		BoxCircle,    // A is the box
		CircleCircle
	};

	struct ContactPoint
	{
		Vector2 LocalPointA; // on A's reference face (box) in A's frame, relative to its centre
		Vector2 LocalPointB; // B's deepest point in B's frame (BoxBox only)
		uint32_t Id = 0;     // which features touch, to find the same point in the next step
		float NormalImpulse = 0.0f;
		float TangentImpulse = 0.0f;

		// Velocity solver data
		Vector2 RA;
		Vector2 RB;
		float NormalMass = 0.0f;
		float TangentMass = 0.0f;
		float VelocityBias = 0.0f;
		float BounceSpeed = 0.0f; // restitution target, set in the first sub-step
	};

	struct Manifold
	{
		uint32_t A = 0; // solver body indices; the normal points from A to B
		uint32_t B = 0;
		ManifoldKind Kind = ManifoldKind::BoxBox;
		Vector2 LocalNormal; // in A's frame (BoxBox, BoxCircle)
		Vector2 Normal;      // world normal while solving velocities
		uint32_t PointCount = 0;
		ContactPoint Points[2];
		float Friction = 0.0f;
		float Restitution = 0.0f;

		// Two points are solved together (Box2D's block solver), which keeps stacks steady.
		bool bBlockSolve = false;
		float K11 = 0.0f, K12 = 0.0f, K22 = 0.0f;              // effective mass matrix
		float InvK11 = 0.0f, InvK12 = 0.0f, InvK22 = 0.0f;     // its inverse (symmetric)
	};

	// Fills manifold.Kind/A/B/LocalNormal/Points (impulses zero). False when not touching.
	// Points up to linearSlop apart are kept (contact margin), so resting contacts don't flicker.
	bool Collide(const SolverBody* bodies, uint32_t indexA, uint32_t indexB, float linearSlop, Manifold& outManifold);

	// The world normal, contact point and separation (negative = overlap) of one point,
	// for the bodies' current positions.
	void GetWorldContact(const Manifold& manifold, const SolverBody& a, const SolverBody& b, uint32_t pointIndex,
		Vector2& outNormal, Vector2& outPoint, float& outSeparation);
}
