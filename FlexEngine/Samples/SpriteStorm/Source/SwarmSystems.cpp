#include "SwarmComponents.h"

#include "Core/Assets/AssetManager.h"
#include "Core/Game/SystemRegistery.h"
#include "Core/Graphics/Camera.h"
#include "Core/Graphics/SpriteRenderer.h"
#include "Core/Math/Math.h"
#include "Core/Physics/Physics2D.h"
#include "Core/World/Transform.h"
#include "Core/World/World.h"

#include <algorithm>
#include <vector>

using namespace Flex;

namespace SpriteStorm
{
	FLEX_REGISTER_COMPONENT(SpriteSwarm)
	FLEX_REGISTER_COMPONENT(Spin)

	// Start: every SpriteSwarm spawns its sprites. They're lightweight entities (no GameObjectInfo),
	// so they don't show up in the Hierarchy and aren't saved; Stop/restart brings back the scene.
	void SpawnSwarms(World& world, float)
	{
		// Read the swarms first: spawning is a structural change, not allowed inside Each.
		std::vector<SpriteSwarm> swarms;
		world.Each<const SpriteSwarm>([&](const SpriteSwarm& swarm) { swarms.push_back(swarm); });

		Rect area{ -640.0f, -360.0f, 1280.0f, 720.0f };
		const Entity cameraEntity = FindMainCamera(world);
		if (const Camera* camera = world.GetComponent<Camera>(cameraEntity)) {
			area = GetCameraViewRect(*world.GetComponent<WorldTransform>(cameraEntity), *camera);
		}

		for (const SpriteSwarm& swarm : swarms) {
			// Each spawned sprite shows the next sprite of the sheet (or the whole texture if it has none).
			std::vector<Uid> sprites = AssetManager::GetSubAssets(swarm.Sheet);
			if (sprites.empty()) {
				sprites.push_back(swarm.Sheet);
			}

			Random random{ swarm.Seed };
			uint32_t next = 0;
			const Rect inside = area.Expanded(-swarm.ColliderRadius * swarm.MaxScale);
			world.SpawnBatch<Transform, WorldTransform, SpriteRenderer, RigidBody2D, CircleCollider2D, Spin>(static_cast<uint32_t>(std::max(swarm.Count, 0)),
				[&](Transform& transform, WorldTransform&, SpriteRenderer& renderer, RigidBody2D& body, CircleCollider2D& collider, Spin& spin) {
					const float scale = random.Range(swarm.MinScale, swarm.MaxScale);
					transform.Position = { random.Range(inside.x, inside.x + inside.width), random.Range(inside.y, inside.y + inside.height) };
					transform.Rotation = random.NextAngle();
					transform.Scale = { scale, scale };

					renderer.AssetId = sprites[next++ % sprites.size()];

					body.Velocity = random.OnUnitCircle() * random.Range(swarm.MinSpeed, swarm.MaxSpeed);
					body.GravityScale = 0.0f;
					body.Restitution = 1.0f;
					body.Friction = 0.0f; // keep bouncing at the same speed
					collider.Radius = swarm.ColliderRadius;
					// Own layer, hitting only the default layer (the walls): sprites never test each other.
					collider.Layer = 2u;
					collider.Mask = DefaultCollisionLayer;

					spin.Speed = random.Range(-swarm.MaxSpin, swarm.MaxSpin);
				});
		}
	}
	FLEX_REGISTER_SYSTEM(Start, SpawnSwarms)

	// Update: spin everything that has a Spin.
	void SpinSprites(World& world, float deltaTime)
	{
		world.Each<Transform, const Spin>([deltaTime](Transform& transform, const Spin& spin) {
			transform.Rotation = Math::WrapAngle(transform.Rotation + spin.Speed * deltaTime);
		});
	}
	FLEX_REGISTER_SYSTEM(Update, SpinSprites)
}
