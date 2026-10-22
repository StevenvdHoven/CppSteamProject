#pragma once
#include "Editor/EditorWindow.h"
#include "Core/World/Entity.h"

namespace Flex
{
	class Archetype;
	struct EditorContext;

	// The "Entities" window: every live entity in the edited scene's world, also the lightweight
	// ones without GameObjectInfo (e.g. the sprites spawned while playing).
	//  - left: one row per archetype (its component set and entity count); open it to list its entities
	//  - right: the selected entity's components, editable like in the Inspector when they have Serialize
	// The filter keeps archetypes that have a component whose name contains the text.
	class EntitiesWindow final : public EditorWindow
	{
	public:
		explicit EntitiesWindow(EditorContext& context);

		void Draw() override;

	private:
		void DrawArchetypes();
		void DrawArchetype(const Archetype& archetype, size_t index);
		void DrawDetails();

	private:
		EditorContext& Context;
		Entity SelectedEntity;
		char Filter[64]{};
	};
}
