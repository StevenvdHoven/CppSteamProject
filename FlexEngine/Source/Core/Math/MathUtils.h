#pragma once
#include <algorithm>
#include <cmath>

// Header-only on purpose: these are tiny and called in hot loops (thousands of
// bullets per frame), so they must inline into game code without a DLL call.
//
// Coordinate convention for all of FlexEngine's 2D math:
//   - x points right, y points DOWN (like SDL, Godot and image files)
//   - angles are in RADIANS in code; a positive angle rotates CLOCKWISE on screen
//   - the editor/scene files may show degrees, converted at the edge
namespace Flex::Math
{
	constexpr float Pi = 3.14159265358979323846f;
	constexpr float TwoPi = Pi * 2.0f;
	constexpr float HalfPi = Pi * 0.5f;
	constexpr float Epsilon = 1e-6f;

	constexpr float DegreesToRadians(float degrees) { return degrees * (Pi / 180.0f); }
	constexpr float RadiansToDegrees(float radians) { return radians * (180.0f / Pi); }

	constexpr float Lerp(float from, float to, float t) { return from + (to - from) * t; }

	// Where value lies between from and to (0..1 when inside). Returns 0 when from == to.
	constexpr float InverseLerp(float from, float to, float value)
	{
		return from != to ? (value - from) / (to - from) : 0.0f;
	}

	constexpr float Remap(float value, float fromMin, float fromMax, float toMin, float toMax)
	{
		return Lerp(toMin, toMax, InverseLerp(fromMin, fromMax, value));
	}

	inline bool NearlyEqual(float a, float b, float tolerance = 1e-5f)
	{
		return std::fabs(a - b) <= tolerance;
	}

	// Moves current towards target by at most maxDelta, without overshooting.
	inline float MoveTowards(float current, float target, float maxDelta)
	{
		if (std::fabs(target - current) <= maxDelta) {
			return target;
		}
		return current + (target > current ? maxDelta : -maxDelta);
	}

	// Wraps an angle into (-Pi, Pi].
	inline float WrapAngle(float radians)
	{
		radians = std::fmod(radians + Pi, TwoPi);
		if (radians <= 0.0f) {
			radians += TwoPi;
		}
		return radians - Pi;
	}

	// Smallest signed difference from one angle to another, in (-Pi, Pi].
	inline float DeltaAngle(float fromRadians, float toRadians)
	{
		return WrapAngle(toRadians - fromRadians);
	}
}
