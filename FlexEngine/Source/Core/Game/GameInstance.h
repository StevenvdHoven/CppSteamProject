#pragma once
#include "Core/Core.h"
#include "Core/Game/SystemRegistery.h"
#include "Core/Graphics/SpriteRenderSystem.h"
#include "Core/Math/Matrix2x3.h"
#include "Core/Math/Vector2.h"
#include "Core/Physics/Physics2D.h"
#include <cstdint>
#include <vector>

namespace Flex
{
	class Scene;
	class SpriteBatch;

	// Runs a scene as a game: the frame loop shared by FlexRuntime and the editor's Play mode.
	//  - Begin: Start systems (and the active project's physics settings)
	//  - Tick: fixed steps (FixedUpdate systems, then PhysicsWorld2D), then Update systems, then
	//    world transforms; pending commands are applied after every system
	//  - Render: the scene as its main Camera sees it
	class FLEX_API GameInstance
	{
	public:
		struct Statistics
		{
			double TickMilliseconds = 0.0;
			double RenderMilliseconds = 0.0;
			uint32_t FixedSteps = 0;      // last tick
			uint32_t SpritesDrawn = 0;    // last render
			PhysicsStats Physics;         // last fixed step
		};

		// At most this many fixed steps per tick, so a slow frame doesn't snowball.
		static constexpr uint32_t MaxFixedStepsPerTick = 5;

		void Begin(Scene& scene);
		void Tick(Scene& scene, float deltaTime);

		// Clears the current render target with the camera's background and draws the sprites,
		// fitted into viewportSize pixels. Without a camera: world origin at the centre, zoom 1.
		void Render(Scene& scene, SpriteBatch& batch, const Vector2& viewportSize);
		// The world -> viewport matrix Render uses.
		Matrix2x3 GetViewMatrix(Scene& scene, const Vector2& viewportSize) const;

		const Statistics& GetStatistics() const { return Stats; }

		// Taken from the active project in Begin (defaults without a project).
		PhysicsSettings Physics;
		PhysicsWorld2D PhysicsWorld;

	private:
		void RunSystems(Scene& scene, const std::vector<SystemInfo>& systems, float deltaTime);

	private:
		std::vector<SystemInfo> FixedSystems;
		std::vector<SystemInfo> UpdateSystems;
		float Accumulator = 0.0f;
		std::vector<SpriteDrawItem> Items;
		Statistics Stats;
	};
}
