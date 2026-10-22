#include "Editor/SceneUndo.h"
#include "Editor/EditorContext.h"
#include "Core/Graphics/SpriteRenderer.h"
#include "Core/Scene/SceneSerializer.h"
#include "Core/World/GameObject.h"

#include <SDL3/SDL.h>

namespace Flex
{
	namespace
	{
		const std::string NoName;
	}

	SceneUndo::State SceneUndo::Capture(EditorContext& context, std::string name) const
	{
		State state;
		SceneSerializer::Save(context.EditorScene, state.Document);
		state.Name = std::move(name);
		state.Revision = context.SceneRevision;
		if (const GameObjectInfo* info = context.EditorScene.GetWorld().GetComponent<GameObjectInfo>(context.CurrentSelection.GetEntity())) {
			state.SelectedId = info->Id;
		}
		return state;
	}

	void SceneUndo::Reset(EditorContext& context)
	{
		States.clear();
		States.push_back(Capture(context, {}));
		Current = 0;
	}

	void SceneUndo::Record(EditorContext& context, std::string name)
	{
		if (States.empty()) {
			Reset(context);
		}
		States.resize(Current + 1); // what could be redone is gone now
		States.push_back(Capture(context, std::move(name)));
		if (States.size() > MaxStates) {
			States.erase(States.begin());
		}
		Current = States.size() - 1;
	}

	const std::string& SceneUndo::GetUndoName() const
	{
		return CanUndo() ? States[Current].Name : NoName;
	}

	const std::string& SceneUndo::GetRedoName() const
	{
		return CanRedo() ? States[Current + 1].Name : NoName;
	}

	void SceneUndo::Undo(EditorContext& context)
	{
		if (CanUndo()) {
			--Current;
			Restore(context, States[Current]);
		}
	}

	void SceneUndo::Redo(EditorContext& context)
	{
		if (CanRedo()) {
			++Current;
			Restore(context, States[Current]);
		}
	}

	void SceneUndo::Restore(EditorContext& context, const State& state)
	{
		std::vector<std::shared_ptr<Asset>> stillUsed;
		context.EditorScene.GetWorld().Each<const SpriteRenderer>([&](const SpriteRenderer& renderer) {
			if (renderer.Loaded) {
				stillUsed.push_back(renderer.Loaded);
			}
		});
		KeepAlive = std::move(stillUsed);

		const bool bAssetSelected = context.CurrentSelection.GetKind() == Selection::Kind::Asset;
		Uid currentlySelected;
		if (const GameObjectInfo* info = context.EditorScene.GetWorld().GetComponent<GameObjectInfo>(context.CurrentSelection.GetEntity())) {
			currentlySelected = info->Id;
		}
		std::vector<std::string> warnings;
		std::string error;
		if (!SceneSerializer::Load(context.EditorScene, state.Document, warnings, error)) {
			SDL_Log("Undo: could not restore the scene: %s", error.c_str());
			return;
		}
		context.SceneRevision = state.Revision;
		// Keep what's selected if it still exists in the restored state; otherwise use the state's selection.
		if (!bAssetSelected) {
			const Entity kept = context.EditorScene.FindByUid(currentlySelected);
			context.CurrentSelection.SelectEntity(kept.IsValid() ? kept : context.EditorScene.FindByUid(state.SelectedId));
		}
	}
}
