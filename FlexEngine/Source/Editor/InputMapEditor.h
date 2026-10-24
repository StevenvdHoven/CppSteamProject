#pragma once
#include "Editor/EditorCore.h"
#include "Editor/EditorWindow.h"
#include "Core/Input/InputMap.h"
#include "Core/Uid.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Flex
{
	class AssetDatabase;
	struct EditorContext;

	// Editor window for one InputMap asset (.inputmap):
	//  - actions on the left (add, rename, type, reorder, delete)
	//  - the selected action's bindings (device, code, component, scale, dead zone), each settable with
	//    Listen: the next key, gamepad button, stick/trigger, or mouse button/wheel on the click box
	//  - a live preview of every action for a chosen player (keyboard and mouse count while this window
	//    is focused; gamepads always), so bindings can be tried without pressing Play.
	// Edits are a working copy until Save (Ctrl+S while focused); the loaded asset is updated in place.
	// Closing with unsaved changes asks first.
	class InputMapEditor final : public AssetEditor
	{
	public:
		InputMapEditor(EditorContext& context, Uid inputMapId);
		~InputMapEditor() override;

		void Draw() override;

		bool HasUnsavedChanges() const { return bDirty; }

		// True while any InputMapEditor waits for an input to bind: the editor's own shortcuts (F5, Ctrl+S, ...)
		// are off then, so they can be bound.
		FLEX_EDITOR_API static bool IsListening();

		// A new .inputmap with a default "Move" + "Jump" in folder (Content right-click > Create, Assets menu):
		// asks for a name, writes the file, refreshes the assets and opens it.
		FLEX_EDITOR_API static void OpenNewInputMapDialog(const std::filesystem::path& folder);
		FLEX_EDITOR_API static void DrawNewInputMapDialog(EditorContext& context);
		FLEX_EDITOR_API static InputMapDefinition MakeDefaultDefinition();

	private:
		void Load();
		bool Save();
		void Changed();

		void DrawToolbar();
		void DrawActions();
		void DrawActionDetails();
		void DrawBinding(InputAction& action, size_t index);
		void DrawPreview();
		void DrawListenPopup();
		void DrawUnsavedChangesPopup();

		void StartListening(int bindingIndex);
		void FinishListening(const InputBinding& binding);
		void StopListening();

	private:
		EditorContext& Context;
		AssetDatabase& Database;
		Uid InputMapId;

		InputMapDefinition Edit;
		bool bDirty = false;
		std::string ErrorMessage;
		std::vector<std::string> Warnings;
		int SelectedAction = -1;
		char CodeFilter[64]{};

		// Live preview: a private copy of the map, evaluated like the game's.
		std::unique_ptr<InputMap> Preview;
		int PreviewPlayer = 0;
		std::vector<std::chrono::steady_clock::time_point> LastPressed;
		std::vector<std::chrono::steady_clock::time_point> LastReleased;

		// Listen
		bool bListening = false;
		bool bRequestListenPopup = false;
		int ListenBinding = -1; // -1 = a new binding
		std::chrono::steady_clock::time_point ListenStart;
		std::array<std::array<float, SDL_GAMEPAD_AXIS_COUNT>, Input::MaxPlayers> ListenAxes{}; // axes when listening started

		bool bRequestUnsavedPopup = false;
	};
}
