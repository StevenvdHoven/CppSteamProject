#pragma once
#include "Core/Math/Vector2.h"
#include "Core/Serialization/Archive.h"
#include "Core/World/ComponentRegistery.h"
#include <cstdint>

// The PhysicsDemo game module. Everything else in the demo (bodies, colliders, mass,
// friction, bounciness, gravity) is set up in the editor; only the launch needs code.
namespace PhysicsDemo
{
	// Put on a GameObject with a RigidBody2D: Delay seconds after the game starts, the body
	// gets Velocity and AngularVelocity (a sleeping body wakes up).
	struct Launcher
	{
		FLEX_COMPONENT(Launcher)
		static constexpr uint32_t Version = 1;

		float Delay = 1.0f;                     // seconds
		Flex::Vector2 Velocity{ 900.0f, 0.0f }; // units per second
		float AngularVelocity = 0.0f;           // radians per second, positive = clockwise

		// Runtime state (not saved).
		float Elapsed = 0.0f;
		bool bLaunched = false;

		void Serialize(Flex::Archive& archive)
		{
			archive.SerializeVersion(Version);
			archive.Field("Delay", Delay);
			archive.Field("Velocity", Velocity);
			archive.Field("AngularVelocity", AngularVelocity);
		}
	};
}
