#pragma once
#include "Core/Math/MathUtils.h"
#include <algorithm>
#include <cstdint>

namespace Flex
{
	// RGBA color with float channels, normally 0..1 (values above 1 are allowed, e.g. for glow).
	// Used for sprite tints, particles, clear colors. ToRGBA8 converts for SDL.
	struct Color
	{
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
		float a = 1.0f;

		struct RGBA8
		{
			uint8_t r, g, b, a;
		};

		constexpr Color() = default;
		constexpr Color(float r, float g, float b, float a = 1.0f) : r{ r }, g{ g }, b{ b }, a{ a } {}

		static constexpr Color White() { return { 1.0f, 1.0f, 1.0f, 1.0f }; }
		static constexpr Color Black() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
		static constexpr Color Clear() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }
		static constexpr Color Red() { return { 1.0f, 0.0f, 0.0f, 1.0f }; }
		static constexpr Color Green() { return { 0.0f, 1.0f, 0.0f, 1.0f }; }
		static constexpr Color Blue() { return { 0.0f, 0.0f, 1.0f, 1.0f }; }
		static constexpr Color Yellow() { return { 1.0f, 1.0f, 0.0f, 1.0f }; }

		static constexpr Color FromRGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
		{
			return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
		}

		// 0xRRGGBBAA, e.g. FromHex(0xFF8800FF) = orange.
		static constexpr Color FromHex(uint32_t rgba)
		{
			return FromRGBA8(static_cast<uint8_t>(rgba >> 24), static_cast<uint8_t>(rgba >> 16), static_cast<uint8_t>(rgba >> 8), static_cast<uint8_t>(rgba));
		}

		// Clamped to 0..255 and rounded.
		RGBA8 ToRGBA8() const
		{
			auto toByte = [](float channel) { return static_cast<uint8_t>(std::clamp(channel, 0.0f, 1.0f) * 255.0f + 0.5f); };
			return { toByte(r), toByte(g), toByte(b), toByte(a) };
		}

		constexpr Color WithAlpha(float alpha) const { return { r, g, b, alpha }; }

		// Channel-wise multiply: tinting.
		constexpr Color operator*(const Color& other) const { return { r * other.r, g * other.g, b * other.b, a * other.a }; }
		constexpr bool operator==(const Color&) const = default;

		static constexpr Color Lerp(const Color& from, const Color& to, float t)
		{
			return { Math::Lerp(from.r, to.r, t), Math::Lerp(from.g, to.g, t), Math::Lerp(from.b, to.b, t), Math::Lerp(from.a, to.a, t) };
		}
	};
}
