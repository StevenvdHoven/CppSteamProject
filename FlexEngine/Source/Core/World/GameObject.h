#pragma once
#include "Core/Serialization/Archive.h"
#include "Core/Uid.h"
#include "Core/World/ComponentRegistery.h"
#include <string>

namespace Flex
{
	// Makes an entity a GameObject: it has a name and a Uid that stays the same in scene
	// files, so other objects (and undo) can find it again. GameObjects are what the
	// Hierarchy shows. The parent/child links live in the transform hierarchy (Hierarchy.h).
	struct GameObjectInfo
	{
		FLEX_COMPONENT(GameObjectInfo)
		static constexpr uint32_t Version = 1;

		std::string Name;
		Uid Id;

		void Serialize(Archive& archive)
		{
			archive.SerializeVersion(Version);
			archive.Field("Name", Name);
			archive.Field("Id", Id);
		}
	};
}
