#include "Vector2.h"
#include <iostream>

using namespace Math;

Vector2::Vector2():
	Vector2(0,0)
{
}

Vector2::Vector2(float x, float y):
	x{x},
	y{y}
{
}

Vector2::Vector2(const Vector2& other):
	x{other.x},
	y{other.y}
{
	
}

Vector2& Vector2::operator=(const Vector2& other)
{
	x = other.x;
	y = other.y;
	return *this;
}

Vector2::Vector2(Vector2&& other) noexcept:
	x{std::move(other.x)},
	y{std::move(other.y)}
{
}

Vector2& Vector2::operator=(Vector2&& other) noexcept
{
	x = std::move(other.x);
	y = std::move(other.y);
	return *this;
}

bool Math::Vector2::operator==(const Vector2& other) const
{
	constexpr float EPSILON = 0.001f;
	return std::fabs(x - other.x) < EPSILON && std::fabs(y - other.y) < EPSILON;
}

Vector2 Vector2::operator+(const Vector2& other) const
{
	return Vector2(x + other.x, y + other.y);
}

Vector2& Vector2::operator+=(const Vector2& other)
{
	x += other.x;
	y += other.y;
	return *this;
}

Vector2 Vector2::operator-(const Vector2& other) const
{
	return Vector2(x - other.x, y - other.y);
}

Vector2& Vector2::operator-=(const Vector2& other)
{
	x -= other.x;
	y -= other.y;
	return *this;
}

Vector2 Vector2::operator*(float scalar) const
{
	return Vector2(x * scalar, y * scalar);
}

Vector2& Vector2::operator*=(float scalar)
{
	x *= scalar;
	y *= scalar;
	return *this;
}

Vector2 Vector2::operator/(float devide) const
{
	return Vector2(x / devide, y / devide);
}

Vector2& Vector2::operator/=(float devide)
{
	x /= devide;
	y /= devide;
	return *this;
}

float Math::Vector2::Length() const
{
	return sqrtf(x * x + y * y);
}

float Vector2::Dot(const Vector2& first, const Vector2& second)
{
	return first.x * second.x + first.y * second.y;
}

float Vector2::Angle(const Vector2& first, const Vector2& second)
{
	return std::atan2(second.y, second.x) - std::atan2(first.y, first.x);
}

float Vector2::DistanceSq(const Vector2& first, const Vector2& second)
{
	return abs(second.x - first.x + second.y - second.y);
}

float Vector2::Distance(const Vector2& first, const Vector2& second)
{
	return sqrtf(DistanceSq(first, second));
}

Math::Vector2 operator*(float scalar, const Math::Vector2& right)
{
	return right * scalar;
}
