#pragma once
#include "Editor/EditorWindow.h"
#include "Core/World/Entity.h"
#include <functional>
#include <unordered_set>
#include <vector>

namespace Flex
{
	struct EditorContext;

	// The "Hierarchy" window: the GameObjects of the edited scene as a tree.
	//  - click selects; drag an object onto another to make it a child, onto empty space to make it a root
	//  - drag an asset from Content onto an object (child) or empty space (root) to create a GameObject
	//  - right-click: create, rename, move up/down, unparent, delete; F2 renames, Delete deletes
	class HierarchyWindow final : public EditorWindow
	{
	public:
		explicit HierarchyWindow(EditorContext& context);

		void Draw() override;

	private:
		void DrawNode(Entity entity);
		void DrawNodeContextMenu(Entity entity);
		void AcceptDrop(Entity target);
		void DrawRenamePopup();
		void StartRename(Entity entity);

	private:
		EditorContext& Context;

		// Changes are queued while the tree is drawn and run afterwards, so the tree
		// isn't modified while it's being walked.
		std::vector<std::function<void()>> PendingActions;

		// When the selection changes (e.g. picked in the Viewport), its parents are opened once.
		Entity RevealedSelection;
		std::unordered_set<Entity> NodesToOpen;

		Entity RenameTarget;
		char RenameBuffer[128]{};
		bool bOpenRenamePopup = false;
	};
}
