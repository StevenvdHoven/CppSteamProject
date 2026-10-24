#pragma once
#include "Editor/EditorCore.h"
#include "Core/Assets/Asset.h"
#include "Core/Serialization/TextDocument.h"
#include "Core/Uid.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Flex
{
	struct EditorContext;

	// Undo/redo for the edited scene, as a list of scene states. After every finished edit
	// the whole scene is captured in the scene file format; undo/redo load an earlier or
	// later state. Simple and can't get an inverse wrong; the cost grows with scene size,
	// which is fine for editor scenes (finer-grained commands can replace it later).
	//
	// GameObjects are found again by Uid, so Entity handles from before an undo/redo are stale.
	class FLEX_EDITOR_API SceneUndo
	{
	public:
		static constexpr size_t MaxStates = 100;

		// Starts a new history with the current scene as its only state (after New/Open).
		void Reset(EditorContext& context);

		// Captures the current scene as a new state named after the edit ("Move Player").
		// Drops anything that could be redone.
		void Record(EditorContext& context, std::string name);

		bool CanUndo() const { return Current > 0; }
		bool CanRedo() const { return Current + 1 < States.size(); }
		// Name of the edit that Undo/Redo would revert/repeat.
		const std::string& GetUndoName() const;
		const std::string& GetRedoName() const;

		void Undo(EditorContext& context);
		void Redo(EditorContext& context);

	private:
		struct State
		{
			TextDocument Document;
			std::string Name;      // the edit that led to this state
			uint64_t Revision = 0; // EditorContext::SceneRevision at capture
			Uid SelectedId;        // selected GameObject at capture (used when the current one is gone)
		};

		State Capture(EditorContext& context, std::string name) const;
		void Restore(EditorContext& context, const State& state);

	private:
		std::vector<State> States;
		size_t Current = 0;

		// Assets used by the scene before a restore, held until the next one, so undo/redo
		// doesn't unload and reload every texture.
		std::vector<std::shared_ptr<Asset>> KeepAlive;
	};
}
