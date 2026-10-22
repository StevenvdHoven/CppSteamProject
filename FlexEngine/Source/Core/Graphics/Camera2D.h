#pragma once
#include "Core/Math/Matrix2x3.h"
#include "Core/Math/Vector2.h"

namespace Flex
{
	// A 2D view: Position is the world point at the centre of the viewport, Zoom is
	// screen pixels per world unit. Used by the editor viewport (game cameras come later).
	struct Camera2D
	{
		Vector2 Position;
		float Zoom = 1.0f;

		// World -> viewport pixels (origin top-left of the viewport).
		Matrix2x3 GetViewMatrix(const Vector2& viewportSize) const
		{
			return { { Zoom, 0.0f }, { 0.0f, Zoom }, viewportSize * 0.5f - Position * Zoom };
		}

		Vector2 WorldToScreen(const Vector2& world, const Vector2& viewportSize) const
		{
			return (world - Position) * Zoom + viewportSize * 0.5f;
		}

		Vector2 ScreenToWorld(const Vector2& screen, const Vector2& viewportSize) const
		{
			return (screen - viewportSize * 0.5f) / Zoom + Position;
		}
	};
}
