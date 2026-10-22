#pragma once
#include "Core/Core.h"
#include "Core/Math/Matrix2x3.h"
#include "Core/Math/Vector2.h"
#include "Core/Serialization/Archive.h"
#include "Core/World/ComponentRegistery.h"
#include "Core/World/Entity.h"

namespace Flex
{
	class World;

	// LOCAL position, rotation and scale: relative to the parent (see Hierarchy.h),
	// or to the world for an entity without a parent. Gameplay and physics write this.
	struct Transform
	{
		FLEX_COMPONENT(Transform)
		static constexpr uint32_t Version = 1;

		Vector2 Position;
		float Rotation = 0.0f; // radians, positive = clockwise on screen
		Vector2 Scale{ 1.0f, 1.0f };

		Matrix2x3 ToMatrix() const { return Matrix2x3::FromTRS(Position, Rotation, Scale); }

		void Serialize(Archive& archive)
		{
			archive.SerializeVersion(Version);
			archive.Field("Position", Position);
			archive.Field("Rotation", Rotation);
			archive.Field("Scale", Scale);
		}
	};

	// Where the entity really is: world = parentWorld * local. Written by UpdateWorldTransforms,
	// read by rendering, physics and the editor. Spawn it together with Transform.
	struct WorldTransform
	{
		FLEX_COMPONENT(WorldTransform)

		Matrix2x3 Matrix;

		const Vector2& GetPosition() const { return Matrix.GetTranslation(); }
		float GetRotation() const { return Matrix.GetRotation(); }
		Vector2 GetScale() const { return Matrix.GetScale(); }
	};

	// Recomputes every WorldTransform: roots first, then children depth-first, so a parent
	// is always done before its children. Always recomputes (no dirty flags).
	// A child whose parent is gone is treated as a root (warning once with FLEX_DEBUG_TOOLS).
	// Also drops dead entities from Children lists.
	FLEX_API void UpdateWorldTransforms(World& world);

	// Computes an entity's world matrix right now by walking up its parents,
	// without relying on WorldTransform being up to date. Identity for a dead entity.
	FLEX_API Matrix2x3 ComputeWorldMatrix(const World& world, Entity entity);
}
