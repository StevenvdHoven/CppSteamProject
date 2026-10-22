#pragma once
#include "Editor/AssetDatabase.h"
#include "Editor/OutputLog.h"
#include "Editor/SceneUndo.h"
#include "Editor/Selection.h"
#include "Editor/ThumbnailCache.h"
#include "Core/Game/GameInstance.h"
#include "Core/Scene/Scene.h"
#include "Core/Uid.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace Flex
{
	// Drag-and-drop payload types (ImGui payload type names are limited to 32 characters).
	inline constexpr const char* AssetPayloadType = "FLEX_ASSET";   // payload: Uid
	inline constexpr const char* EntityPayloadType = "FLEX_ENTITY"; // payload: Entity (a GameObject in EditorScene)

	// Shared state and services for the editor windows of one project.
	// Owned by EditorApp; windows keep a reference.
	struct EditorContext
	{
		explicit EditorContext(AssetDatabase& database)
			: Assets{ database }
			, Thumbnails{ database }
		{
		}

		AssetDatabase& Assets;
		ThumbnailCache Thumbnails;
		Selection CurrentSelection;

		// The scene being edited. Change it through SceneEditing so every window stays in sync.
		Scene EditorScene;
		std::filesystem::path ScenePath; // the .scene file (absolute); empty until saved once

		// Every change gets a new revision; the scene is dirty when it differs from the saved one.
		// Undo/redo go back to a state's revision, so undoing to the saved state is clean again.
		uint64_t SceneRevision = 0;
		uint64_t SavedSceneRevision = 0;
		uint64_t RevisionCounter = 0;
		void MarkSceneChanged() { SceneRevision = ++RevisionCounter; }
		bool IsSceneDirty() const { return SceneRevision != SavedSceneRevision; }

		SceneUndo Undo;

		// Play mode: the scene runs as a game (Start/FixedUpdate/Update systems, physics).
		// Stopping restores the scene as it was, so changes made while playing are discarded.
		GameInstance Game;
		bool bIsPlaying = false;
		bool bGameView = false;               // Viewport shows what the main camera sees
		std::function<void()> TogglePlay;     // set by EditorApp
		std::string GameModuleStatus;         // shown in the Project window

		// Increases on every asset refresh, so windows can rebuild cached lists.
		uint32_t AssetRevision = 0;

		// Build output and engine log lines, shown by the Output window.
		OutputLog Output;
		std::string ScriptJobStatus; // "Building SpriteStorm.dll..." while a build runs

		// Set by EditorApp.
		std::function<void(Uid)> OpenAsset;     // double-click: opens the right editor for the asset's type
		std::function<void()> RefreshAssets;    // rescans Content/
		std::function<void()> NewScript;        // opens the New Script dialog
		std::function<void()> RecompileScripts; // builds the game module
	};
}
