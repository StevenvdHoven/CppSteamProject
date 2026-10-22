#include "Core/Graphics/Camera.h"
#include "Core/World/Transform.h"
#include "Core/World/World.h"
#include <algorithm>

namespace Flex
{
	FLEX_REGISTER_COMPONENT(Camera)

	Entity FindMainCamera(World& world)
	{
		Entity found;
		world.Each<const Camera, const WorldTransform>([&](Entity entity, const Camera&, const WorldTransform&) {
			if (!found.IsValid() || entity.Index < found.Index) {
				found = entity; // lowest index: stable when several exist
			}
		});
		return found;
	}

	Rect GetCameraViewRect(const WorldTransform& worldTransform, const Camera& camera)
	{
		return Rect::FromCenter(worldTransform.GetPosition(), camera.ViewSize);
	}

	Camera2D FitCamera(const WorldTransform& worldTransform, const Camera& camera, const Vector2& viewportSize)
	{
		Camera2D result;
		result.Position = worldTransform.GetPosition();
		if (camera.ViewSize.x > 0.0f && camera.ViewSize.y > 0.0f) {
			result.Zoom = std::min(viewportSize.x / camera.ViewSize.x, viewportSize.y / camera.ViewSize.y);
		}
		return result;
	}
}
