#pragma once
#include "Core/Core.h"
#include "Core/Graphics/Camera2D.h"
#include "Core/Math/Color.h"
#include "Core/Math/Rect.h"
#include "Core/Math/Vector2.h"
#include "Core/Serialization/Archive.h"
#include "Core/World/ComponentRegistery.h"
#include "Core/World/Entity.h"

namespace Flex
{
	class World;
	struct WorldTransform;

	// What the game shows: ViewSize world units around the entity's position, scaled to fit
	// the window (letterboxed, aspect kept). The editor draws this rectangle in the Viewport.
	struct Camera
	{
		FLEX_COMPONENT(Camera)
		static constexpr uint32_t Version = 1;

		Vector2 ViewSize{ 1280.0f, 720.0f };
		Color Background{ 0.07f, 0.08f, 0.10f, 1.0f };

		void Serialize(Archive& archive)
		{
			archive.SerializeVersion(Version);
			archive.Field("ViewSize", ViewSize);
			archive.Field("Background", Background);
		}
	};

	// The first entity with Camera + WorldTransform, or Entity{} when there is none.
	FLEX_API Entity FindMainCamera(World& world);

	// The world rectangle the camera shows.
	FLEX_API Rect GetCameraViewRect(const WorldTransform& worldTransform, const Camera& camera);

	// A Camera2D that fits the camera's view into a viewport of viewportSize pixels.
	FLEX_API Camera2D FitCamera(const WorldTransform& worldTransform, const Camera& camera, const Vector2& viewportSize);
}
