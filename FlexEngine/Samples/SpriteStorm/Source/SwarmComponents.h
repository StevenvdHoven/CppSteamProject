#pragma once
#include "Core/Assets/Texture.h"
#include "Core/Serialization/Archive.h"
#include "Core/Uid.h"
#include "Core/World/ComponentRegistery.h"
#include <cstdint>

// The SpriteStorm game module: game code written the way a FlexEngine user writes it.
// Components are plain structs; FLEX_REGISTER_COMPONENT (in SwarmSystems.cpp) makes them
// show up in the editor's Add Component and load from scene files.
namespace SpriteStorm
{
	// Put on a GameObject in the scene. When the game starts, it spawns Count sprites inside
	// the main camera's view, each showing the next sprite of Sheet, moving in a random
	// direction and bouncing off the scene's static colliders (the walls).
	struct SpriteSwarm
	{
		FLEX_COMPONENT(SpriteSwarm)
		static constexpr uint32_t Version = 1;

		Flex::Uid Sheet;             // a texture with sprites cut from it
		int32_t Count = 10000;
		float MinSpeed = 60.0f;      // units per second
		float MaxSpeed = 260.0f;
		float MinScale = 0.6f;
		float MaxScale = 1.1f;
		float MaxSpin = 2.0f;        // radians per second, either way
		float ColliderRadius = 8.0f; // in sprite pixels (scaled with the sprite)
		uint32_t Seed = 2026;

		void Serialize(Flex::Archive& archive)
		{
			archive.SerializeVersion(Version);
			archive.Field("Sheet", Sheet, { Flex::Texture::TypeName });
			archive.Field("Count", Count);
			archive.Field("MinSpeed", MinSpeed);
			archive.Field("MaxSpeed", MaxSpeed);
			archive.Field("MinScale", MinScale);
			archive.Field("MaxScale", MaxScale);
			archive.Field("MaxSpin", MaxSpin);
			archive.Field("ColliderRadius", ColliderRadius);
			archive.Field("Seed", Seed);
		}
	};

	// Spins an entity. Only on the spawned sprites, so it has no Serialize (never saved).
	struct Spin
	{
		FLEX_COMPONENT(Spin)
		float Speed = 0.0f; // radians per second
	};
}
