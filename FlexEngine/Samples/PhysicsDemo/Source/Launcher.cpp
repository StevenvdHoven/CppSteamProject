#include "Launcher.h"
#include "Core/Game/SystemRegistery.h"
#include "Core/Physics/Physics2D.h"
#include "Core/World/World.h"

using namespace Flex;

namespace PhysicsDemo
{
	FLEX_REGISTER_COMPONENT(Launcher)

	// FixedUpdate (before physics): launches every Launcher once its delay has passed.
	void LaunchBodies(World& world, float deltaTime)
	{
		world.Each<Launcher, RigidBody2D>([deltaTime](Launcher& launcher, RigidBody2D& body) {
			if (launcher.bLaunched) {
				return;
			}
			launcher.Elapsed += deltaTime;
			if (launcher.Elapsed >= launcher.Delay) {
				body.Velocity = launcher.Velocity;
				body.AngularVelocity = launcher.AngularVelocity;
				launcher.bLaunched = true;
			}
		});
	}
	FLEX_REGISTER_SYSTEM(FixedUpdate, LaunchBodies)
}
