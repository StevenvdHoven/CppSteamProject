#pragma once
#include "Core/Math/Vector2.h"
#include <cstdint>

namespace Flex
{
	// Small, fast, seedable random number generator (PCG32, pcg-random.org).
	// Same seed = same sequence on every machine, so gameplay can be deterministic
	// (replays, RTS lockstep). Much lighter than std::mt19937 for bullet-heavy code.
	//
	// Not thread-safe: give each system/thread its own Random. Not for cryptography.
	class Random
	{
	public:
		explicit Random(uint64_t seed = 0x853c49e6748fea9bULL, uint64_t stream = 0xda3e39cb94b95bdbULL)
		{
			Seed(seed, stream);
		}

		void Seed(uint64_t seed, uint64_t stream = 0xda3e39cb94b95bdbULL)
		{
			State = 0;
			Increment = (stream << 1u) | 1u;
			NextUInt();
			State += seed;
			NextUInt();
		}

		uint32_t NextUInt()
		{
			const uint64_t oldState = State;
			State = oldState * 6364136223846793005ULL + Increment;
			const uint32_t xorShifted = static_cast<uint32_t>(((oldState >> 18u) ^ oldState) >> 27u);
			const uint32_t rotation = static_cast<uint32_t>(oldState >> 59u);
			return (xorShifted >> rotation) | (xorShifted << ((32u - rotation) & 31u));
		}

		// Uniform in [0, bound) without modulo bias. bound 0 returns 0.
		uint32_t NextUInt(uint32_t bound)
		{
			if (bound == 0) {
				return 0;
			}
			const uint32_t threshold = (0u - bound) % bound;
			for (;;) {
				const uint32_t value = NextUInt();
				if (value >= threshold) {
					return value % bound;
				}
			}
		}

		// Uniform in [minInclusive, maxInclusive].
		int32_t Range(int32_t minInclusive, int32_t maxInclusive)
		{
			if (maxInclusive <= minInclusive) {
				return minInclusive;
			}
			const uint64_t span = static_cast<uint64_t>(static_cast<int64_t>(maxInclusive) - minInclusive) + 1;
			if (span > 0xFFFFFFFFull) {
				return static_cast<int32_t>(NextUInt()); // the full 32-bit range
			}
			return static_cast<int32_t>(minInclusive + static_cast<int64_t>(NextUInt(static_cast<uint32_t>(span))));
		}

		// Uniform in [0, 1).
		float NextFloat()
		{
			return static_cast<float>(NextUInt() >> 8) * (1.0f / 16777216.0f);
		}

		// Uniform in [min, max).
		float Range(float min, float max)
		{
			return min + (max - min) * NextFloat();
		}

		bool Chance(float probability)
		{
			return NextFloat() < probability;
		}

		// Uniform angle in [0, 2 Pi).
		float NextAngle()
		{
			return NextFloat() * Math::TwoPi;
		}

		// Random direction of length 1.
		Vector2 OnUnitCircle()
		{
			return Vector2::FromAngle(NextAngle());
		}

		// Uniform point inside the unit circle (rejection sampling, ~1.27 tries on average).
		Vector2 InsideUnitCircle()
		{
			for (;;) {
				const Vector2 point{ Range(-1.0f, 1.0f), Range(-1.0f, 1.0f) };
				if (point.LengthSquared() <= 1.0f) {
					return point;
				}
			}
		}

	private:
		uint64_t State = 0;
		uint64_t Increment = 1;
	};
}
