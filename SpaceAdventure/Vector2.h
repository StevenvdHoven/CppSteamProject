#pragma once

namespace Math
{
	struct Vector2
	{
		Vector2();
		Vector2(float x, float y);

		Vector2(const Vector2& other);
		Vector2& operator=(const Vector2& other);

		Vector2(Vector2&& other) noexcept;
		Vector2& operator=(Vector2&& other) noexcept;

		Vector2 operator+(const Vector2& other);
		Vector2& operator+=(const Vector2& other);

		Vector2 operator-(const Vector2& other);
		Vector2& operator-=(const Vector2& other);

		Vector2 operator*(float scalar);
		Vector2& operator*=(float scalar);

		Vector2 operator/(float devide);
		Vector2& operator/=(float devide);

		static float Dot(const Vector2& first, const Vector2& second);
		static float Angle(const Vector2& first, const Vector2& second);
		static float DistanceSq(const Vector2& first, const Vector2& second);
		static float Distance(const Vector2& first, const Vector2& second);

		float x;
		float y;
	};
}

