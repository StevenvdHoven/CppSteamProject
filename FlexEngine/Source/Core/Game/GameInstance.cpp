#include "Core/Game/GameInstance.h"
#include "Core/Graphics/Camera.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Graphics/SpriteBatch.h"
#include "Core/Project.h"
#include "Core/Scene/Scene.h"
#include "Core/World/Transform.h"
#include "Core/World/World.h"
#include <SDL3/SDL.h>
#include <algorithm>

namespace Flex
{
	namespace
	{
		double MillisecondsSince(uint64_t start)
		{
			return static_cast<double>(SDL_GetPerformanceCounter() - start) * 1000.0 / static_cast<double>(SDL_GetPerformanceFrequency());
		}
	}

	void GameInstance::RunSystems(Scene& scene, const std::vector<SystemInfo>& systems, float deltaTime)
	{
		World& world = scene.GetWorld();
		for (const SystemInfo& system : systems) {
			system.Function(world, deltaTime);
			world.ApplyCommands();
		}
	}

	void GameInstance::Begin(Scene& scene)
	{
		FixedSystems = SystemRegistery::GetSystems(SystemPhase::FixedUpdate);
		UpdateSystems = SystemRegistery::GetSystems(SystemPhase::Update);
		Accumulator = 0.0f;
		Stats = {};
		if (const Project* project = Project::GetActive()) {
			Physics = project->GetPhysicsSettings();
		}
		PhysicsWorld.Reset();

		UpdateWorldTransforms(scene.GetWorld()); // Start systems see correct world positions
		RunSystems(scene, SystemRegistery::GetSystems(SystemPhase::Start), 0.0f);
		UpdateWorldTransforms(scene.GetWorld());
	}

	void GameInstance::Tick(Scene& scene, float deltaTime)
	{
		const uint64_t start = SDL_GetPerformanceCounter();
		World& world = scene.GetWorld();
		const float step = Physics.FixedTimeStep > 0.0f ? Physics.FixedTimeStep : 1.0f / 60.0f;

		Accumulator += std::max(deltaTime, 0.0f);
		Stats.FixedSteps = 0;
		while (Accumulator >= step && Stats.FixedSteps < MaxFixedStepsPerTick) {
			RunSystems(scene, FixedSystems, step);
			Stats.Physics = PhysicsWorld.Step(world, step, Physics);
			Accumulator -= step;
			++Stats.FixedSteps;
		}
		if (Stats.FixedSteps == MaxFixedStepsPerTick) {
			Accumulator = std::min(Accumulator, step); // drop the backlog instead of catching up forever
		}

		RunSystems(scene, UpdateSystems, deltaTime);
		UpdateWorldTransforms(world);
		Stats.TickMilliseconds = MillisecondsSince(start);
	}

	Matrix2x3 GameInstance::GetViewMatrix(Scene& scene, const Vector2& viewportSize) const
	{
		World& world = scene.GetWorld();
		const Entity cameraEntity = FindMainCamera(world);
		const Camera* camera = world.GetComponent<Camera>(cameraEntity);
		const WorldTransform* cameraTransform = world.GetComponent<WorldTransform>(cameraEntity);
		if (!camera || !cameraTransform) {
			return Camera2D{}.GetViewMatrix(viewportSize);
		}
		return FitCamera(*cameraTransform, *camera, viewportSize).GetViewMatrix(viewportSize);
	}

	void GameInstance::Render(Scene& scene, SpriteBatch& batch, const Vector2& viewportSize)
	{
		const uint64_t start = SDL_GetPerformanceCounter();
		World& world = scene.GetWorld();

		Color background{ 0.0f, 0.0f, 0.0f, 1.0f };
		if (const Camera* camera = world.GetComponent<Camera>(FindMainCamera(world))) {
			background = camera->Background;
		}
		SDL_Renderer* sdlRenderer = Renderer::GetSDLRenderer();
		SDL_SetRenderDrawColorFloat(sdlRenderer, background.r, background.g, background.b, background.a);
		SDL_RenderClear(sdlRenderer);

		CollectSprites(world, Items);
		DrawSprites(Items, batch, GetViewMatrix(scene, viewportSize));
		batch.Flush();
		Stats.SpritesDrawn = static_cast<uint32_t>(Items.size());
		Stats.RenderMilliseconds = MillisecondsSince(start);
	}
}
