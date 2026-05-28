#pragma once
#include "Math.h"

namespace Math {
	class Matrix2x3
	{
	public:
		Matrix2x3();
		Matrix2x3(const Vector2& dirX, const Vector2& dirY, const Vector2& origin);

		Vector2 Transform(const Vector2& vec);

		static Matrix2x3 FromTranslation(const Vector2& offset);
		static Matrix2x3 FromRotation(float degrees);
		static Matrix2x3 FromScale(float scale);
		static Matrix2x3 FromScale(const Vector2& scale);
		static Matrix2x3 FromScale(float scalex, float scaley);

		float ExtractScale() const { return dirX.Length(); }

		float Determinant() const;
		Matrix2x3 Inverse() const;
		
		Vector2 dirX;
		Vector2 dirY;
		Vector2 orig;

		Matrix2x3 operator*(const Matrix2x3& rhs);
	};
}


