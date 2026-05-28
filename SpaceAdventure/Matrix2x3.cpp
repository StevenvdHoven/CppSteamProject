#include "Matrix2x3.h"
#include "iostream"
#include <cmath>
using namespace Math;

Matrix2x3::Matrix2x3()
{
}

Matrix2x3::Matrix2x3(const Vector2& dirX, const Vector2& dirY, const Vector2& origin):
	dirX{dirX},
	dirY{dirY},
	orig{origin}
{
}

Vector2 Math::Matrix2x3::Transform(const Vector2& vec)
{
	return Vector2{ vec.x * dirX + vec.y * dirY + orig};
}

Matrix2x3 Math::Matrix2x3::FromTranslation(const Vector2& offset)
{
	return Matrix2x3(Vector2{1.0f, 0.0f},Vector2{0.0f, 1.0f}, offset);
}

Matrix2x3 Math::Matrix2x3::FromRotation(float degrees)
{
	float radians = DEG2RAD(degrees);
	return Matrix2x3(
		Vector2{ std::cosf(radians), std::sinf(radians) },
		Vector2{ -std::sinf(radians), std::cos(radians) },
		Vector2{}
	);
}

Matrix2x3 Math::Matrix2x3::FromScale(float scale)
{
	return FromScale(Vector2{ scale, scale });
}

Matrix2x3 Math::Matrix2x3::FromScale(const Vector2& scale)
{
	return FromScale(scale.x, scale.y);
}

Matrix2x3 Math::Matrix2x3::FromScale(float scalex, float scaley)
{
	return Matrix2x3(
		Vector2{ scalex, 0.0f },
		Vector2{ 0.0f, scaley },
		Vector2{}
	);
}

float Math::Matrix2x3::Determinant() const
{
	return dirX.x * dirY.y - dirX.y * dirY.x;
}

Matrix2x3 Math::Matrix2x3::Inverse() const
{
	float det = Determinant();
	return Matrix2x3{
		Vector2(+dirY.y, -dirX.y) / det,
		Vector2(-dirY.x, +dirX.x) / det,
		Vector2(dirY.x * orig.y - dirY.y * orig.x, -(dirX.x * orig.y - dirX.y * orig.x)) / det
	};
}

Matrix2x3 Math::Matrix2x3::operator*(const Matrix2x3& rhs)
{
	return Matrix2x3{
		Vector2{ rhs.dirX.x * dirX.x + rhs.dirX.y * dirY.x, rhs.dirX.x * dirX.y + rhs.dirX.y * dirY.y },
		Vector2{ rhs.dirY.x * dirX.x + rhs.dirY.y * dirY.x, rhs.dirY.x * dirX.y + rhs.dirY.y * dirY.y },
		Vector2{ rhs.orig.x * dirX.x + rhs.orig.y * dirY.x + orig.x, rhs.orig.x * dirX.y + rhs.orig.y * dirY.y + orig.y }
	};
}
