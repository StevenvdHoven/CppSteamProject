#include "Editor/EditorApp.h"
#include "Editor/ContentBrowser.h"
#include "Editor/EditorAssetSource.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorPreferences.h"
#include "Editor/EditorWindow.h"
#include "Editor/HierarchyWindow.h"
#include "Editor/InspectorWindow.h"
#include "Editor/ProjectBrowser.h"
#include "Editor/TextureEditor.h"
#include "Editor/TextureImporter.h"
#include "Editor/ViewportWindow.h"
#include "Core/Assets/AssetManager.h"
#include "Core/Engine.h"
#include "Core/FileSystem.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Graphics/SpriteRenderSystem.h"
#include "Core/Project.h"
#include "Core/Scene/SceneSerializer.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Flex
{
	namespace
	{
		// Project name, folder and frame rate.
		class ProjectWindow final : public EditorWindow
		{
		public:
			ProjectWindow() : EditorWindow("Project") {}

			void Draw() override
			{
				if (BeginWindow()) {
					const Project* project = Project::GetActive();
					ImGui::Text("Name: %s", project->GetName().c_str());
					ImGui::Text("Folder: %s", PathToUtf8(project->GetRootDirectory()).c_str());
					ImGui::Text("Created with engine: %s", project->GetEngineVersion().c_str());
					ImGui::Separator();
					ImGui::Text("FlexEngine %s (%s)", Engine::GetVersion(), Engine::GetBuildConfigName());
					ImGui::Text("%.1f FPS (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
				}
				EndWindow();
			}
		};

		constexpr const char* WindowSettingsName = "FlexEditorWindows";
		constexpr const char* SceneAssetType = SceneSerializer::AssetTypeName;
		constexpr const char* SavePromptPopup = "Save changes?";
		constexpr const char* SceneErrorPopup = "Scene error";

		// Settings entries in EditorLayout.ini.
		void* const OpenWindowsEntry = reinterpret_cast<void*>(1);
		void* const LastSceneEntry = reinterpret_cast<void*>(2);
	}

	EditorApp::EditorApp() = default;
	EditorApp::~EditorApp() = default;

	bool EditorApp::Init(const std::filesystem::path& projectFile)
	{
		if (!Engine::Init()) {
			return false;
		}

		std::string startupError;
		if (!projectFile.empty()) {
			if (Project::Open(projectFile, startupError)) {
				EditorPreferences preferences;
				preferences.AddRecentProject(Project::GetActive()->GetProjectFile());
			}
			else {
				SDL_Log("Could not open project: %s", startupError.c_str());
			}
		}

		if (Project* project = Project::GetActive()) {
			const std::string title = "FlexEngine - " + project->GetName() + " (" + Engine::GetBuildConfigName() + ")";
			const std::filesystem::path layoutFile = project->GetSavedDirectory() / "EditorLayout.ini";
			std::error_code error;
			bResetLayoutRequested = !std::filesystem::exists(layoutFile, error); // first time: default layout
			if (!CreateWindowAndImGui(title, 1600, 900, layoutFile)) {
				return false;
			}

			TextureImporter::Register(Assets);
			Assets.RegisterType(SceneAssetType, { SceneSerializer::FileExtension });
			Assets.SetOwnerCacheFile(project->GetSavedDirectory() / "AssetOwners.cache");
			AssetSourceForEditor = std::make_unique<EditorAssetSource>(Assets);
			AssetManager::SetSource(AssetSourceForEditor.get());

			Context = std::make_unique<EditorContext>(Assets);
			Context->OpenAsset = [this](Uid assetId) { OpenAsset(assetId); };
			Context->RefreshAssets = [this]() { RefreshAssets(); };
			CreateProjectWindows();
			Context->Undo.Reset(*Context);
			RegisterWindowSettingsHandler(); // before the first frame, when ImGui reads the .ini
			RefreshAssets();
		}
		else {
			const std::string title = std::string("FlexEngine - Project Browser (") + Engine::GetBuildConfigName() + ")";
			if (!CreateWindowAndImGui(title, 1000, 600, {})) {
				return false;
			}
			Browser = std::make_unique<ProjectBrowser>(*this);
			if (!startupError.empty()) {
				Browser->ShowError(startupError);
			}
		}

		bIsRunning = true;
		return true;
	}

	void EditorApp::CreateProjectWindows()
	{
		// Order = Windows menu order and draw order. Content after Project: a window that appears
		// last in a dock node becomes its visible tab.
		Panels.push_back(std::make_unique<HierarchyWindow>(*Context));
		Panels.push_back(std::make_unique<ViewportWindow>(*Context));
		Panels.push_back(std::make_unique<InspectorWindow>(*Context));
		Panels.push_back(std::make_unique<ProjectWindow>());
		Panels.push_back(std::make_unique<ContentBrowser>(*Context));

		// Which editor opens which asset type. Sprites open their texture's editor with the sprite selected.
		AssetOpeners[TextureImporter::TypeName] = [this](const AssetInfo& asset) {
			OpenAssetEditor(asset.Id, {}, [this, &asset]() { return std::make_unique<TextureEditor>(*Context, asset.Id); });
		};
		AssetOpeners[TextureImporter::SpriteTypeName] = [this](const AssetInfo& asset) {
			const Uid textureId = asset.ParentId;
			const Uid spriteId = asset.Id;
			OpenAssetEditor(textureId, spriteId, [this, textureId, spriteId]() { return std::make_unique<TextureEditor>(*Context, textureId, spriteId); });
		};
		// A scene opens in the Hierarchy/Viewport (after asking to save the current one).
		AssetOpeners[SceneAssetType] = [this](const AssetInfo& asset) {
			const std::filesystem::path sceneFile = Assets.GetSourcePath(asset);
			RunAfterSavePrompt([this, sceneFile]() { OpenScene(sceneFile); });
		};
	}

	// Stores which windows are open and the last scene in the project's EditorLayout.ini, next to ImGui's own layout:
	//     [FlexEditorWindows][Open]
	//     Content=1
	//     [FlexEditorWindows][Scene]
	//     Path=Content/Main.scene
	void EditorApp::RegisterWindowSettingsHandler()
	{
		ImGuiSettingsHandler handler;
		handler.TypeName = WindowSettingsName;
		handler.TypeHash = ImHashStr(WindowSettingsName);
		handler.UserData = this;
		handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char* name) -> void* {
			if (std::strcmp(name, "Open") == 0) {
				return OpenWindowsEntry;
			}
			return std::strcmp(name, "Scene") == 0 ? LastSceneEntry : nullptr;
		};
		handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler* settingsHandler, void* entry, const char* line) {
			EditorApp* app = static_cast<EditorApp*>(settingsHandler->UserData);
			const char* separator = std::strchr(line, '=');
			if (!separator) {
				return;
			}
			const std::string key(line, separator);
			if (entry == LastSceneEntry) {
				if (key == "Path" && separator[1] != '\0') {
					app->StartupScene = Project::GetActive()->GetRootDirectory() / Utf8ToPath(separator + 1);
				}
			} else if (EditorWindow* panel = app->FindPanel(key)) {
				panel->SetOpen(std::atoi(separator + 1) != 0);
			}
		};
		handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* settingsHandler, ImGuiTextBuffer* buffer) {
			const EditorApp* app = static_cast<EditorApp*>(settingsHandler->UserData);
			if (!app->Context) {
				return;
			}
			buffer->appendf("[%s][Open]\n", settingsHandler->TypeName);
			for (const auto& panel : app->Panels) {
				buffer->appendf("%s=%d\n", panel->GetTitle().c_str(), panel->IsOpen() ? 1 : 0);
			}
			buffer->append("\n");
			if (!app->Context->ScenePath.empty()) {
				std::error_code error;
				const std::filesystem::path relative = std::filesystem::relative(app->Context->ScenePath, Project::GetActive()->GetRootDirectory(), error);
				buffer->appendf("[%s][Scene]\nPath=%s\n\n", settingsHandler->TypeName, PathToGenericUtf8(relative).c_str());
			}
		};
		ImGui::AddSettingsHandler(&handler);
	}

	EditorWindow* EditorApp::FindPanel(const std::string& title) const
	{
		for (const auto& panel : Panels) {
			if (panel->GetTitle() == title) {
				return panel.get();
			}
		}
		return nullptr;
	}

	// Hierarchy on the left, Inspector on the right, Content (and Project) at the bottom,
	// Viewport in the middle. Windows that don't exist yet keep their spot for when they do.
	void EditorApp::BuildDefaultLayout(ImGuiID dockspaceId)
	{
		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

		ImGuiID center = dockspaceId;
		const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
		const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, nullptr, &center);
		const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.32f, nullptr, &center);

		ImGui::DockBuilderDockWindow("Hierarchy", left);
		ImGui::DockBuilderDockWindow("Inspector", right);
		ImGui::DockBuilderDockWindow("Content", bottom);
		ImGui::DockBuilderDockWindow("Project", bottom);
		ImGui::DockBuilderDockWindow("Viewport", center);
		ImGui::DockBuilderFinish(dockspaceId);

		for (const auto& panel : Panels) {
			panel->SetOpen(true);
		}
	}

	bool EditorApp::CreateWindowAndImGui(const std::string& title, int width, int height, const std::filesystem::path& imguiIniFile)
	{
		const float displayScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
		Window = SDL_CreateWindow(title.c_str(), static_cast<int>(width * displayScale), static_cast<int>(height * displayScale), SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
		if (!Window) {
			SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
			return false;
		}

		if (!Renderer::Init(Window)) {
			return false;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// Layout is saved per project (in Saved/). The project browser saves no layout.
		ImGuiIniFile = PathToUtf8(imguiIniFile);
		io.IniFilename = ImGuiIniFile.empty() ? nullptr : ImGuiIniFile.c_str();

		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(displayScale);
		style.FontScaleDpi = displayScale;

		ImGui_ImplSDL3_InitForSDLRenderer(Window, Renderer::GetSDLRenderer());
		ImGui_ImplSDLRenderer3_Init(Renderer::GetSDLRenderer());
		return true;
	}

	void EditorApp::Run()
	{
		ImGuiIO& io = ImGui::GetIO();

		while (bIsRunning)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				ImGui_ImplSDL3_ProcessEvent(&event);
				if (event.type == SDL_EVENT_QUIT) {
					RequestQuit();
				}
				if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(Window)) {
					RequestQuit();
				}
			}

			if (SDL_GetWindowFlags(Window) & SDL_WINDOW_MINIMIZED) {
				SDL_Delay(10);
				continue;
			}

			ImGui_ImplSDLRenderer3_NewFrame();
			ImGui_ImplSDL3_NewFrame();
			ImGui::NewFrame();

			if (Browser) {
				Browser->Draw();
			}
			else {
				DrawProjectEditor();
			}

			ImGui::Render();
			SDL_Renderer* sdlRenderer = Renderer::GetSDLRenderer();
			SDL_SetRenderScale(sdlRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
			SDL_SetRenderDrawColor(sdlRenderer, 30, 30, 30, 255);
			SDL_RenderClear(sdlRenderer);
			ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdlRenderer);
			SDL_RenderPresent(sdlRenderer);
		}
	}

	void EditorApp::Shutdown()
	{
		Browser.reset();

		// Save the layout while the windows still exist: the settings handler writes their state.
		if (ImGui::GetCurrentContext() && ImGui::GetIO().IniFilename) {
			ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
			ImGui::GetIO().IniFilename = nullptr;
		}

		// Release every texture before the renderer goes away.
		AssetEditors.clear();
		Panels.clear();
		Context.reset();
		AssetManager::Shutdown();
		AssetSourceForEditor.reset();

		if (ImGui::GetCurrentContext()) {
			ImGui_ImplSDLRenderer3_Shutdown();
			ImGui_ImplSDL3_Shutdown();
			ImGui::DestroyContext();
		}

		Renderer::Shutdown();
		if (Window) {
			SDL_DestroyWindow(Window);
			Window = nullptr;
		}

		Project::CloseActive();
		Engine::Shutdown();
	}

	void EditorApp::RequestQuit()
	{
		if (Context) {
			RunAfterSavePrompt([this]() { bIsRunning = false; });
		} else {
			bIsRunning = false;
		}
	}

	bool EditorApp::LaunchEditor(const std::filesystem::path& projectFile)
	{
		// SDL_GetBasePath is the folder of the running exe, including a trailing separator.
		const char* basePath = SDL_GetBasePath();
		if (!basePath) {
			SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
			return false;
		}

		const std::string editorExe = std::string(basePath) + "FlexEditor.exe";
		const std::string projectArgument = PathToUtf8(projectFile);

		std::vector<const char*> arguments{ editorExe.c_str() };
		if (!projectFile.empty()) {
			arguments.push_back("--project");
			arguments.push_back(projectArgument.c_str());
		}
		arguments.push_back(nullptr);

		SDL_Process* process = SDL_CreateProcess(arguments.data(), false);
		if (!process) {
			SDL_Log("Could not start %s: %s", editorExe.c_str(), SDL_GetError());
			return false;
		}

		// Only frees SDL's handle; the new editor keeps running on its own.
		SDL_DestroyProcess(process);
		return true;
	}

	void EditorApp::DrawMainMenuBar()
	{
		if (!ImGui::BeginMainMenuBar()) {
			return;
		}

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene")) {
				RunAfterSavePrompt([this]() { NewScene(); });
			}
			if (ImGui::MenuItem("Open Scene...", nullptr, false, !OpenSceneDialog.IsOpen())) {
				OpenSceneDialog.OpenFile(Window, "FlexEngine Scene", "scene", Project::GetActive()->GetContentDirectory());
			}
			if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
				SaveScene();
			}
			if (ImGui::MenuItem("Save Scene As...", nullptr, false, !SaveSceneDialog.IsOpen())) {
				SaveSceneDialog.SaveFile(Window, "FlexEngine Scene", "scene", Project::GetActive()->GetContentDirectory());
			}
			ImGui::Separator();
			if (ImGui::MenuItem("New Project...")) {
				LaunchEditor({});
			}
			if (ImGui::MenuItem("Open Project...", nullptr, false, !OpenProjectDialog.IsOpen())) {
				OpenProjectDialog.OpenFile(Window, "FlexEngine Project", "flexproj", Project::GetActive()->GetRootDirectory().parent_path());
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) {
				RequestQuit();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			const SceneUndo& undo = Context->Undo;
			if (ImGui::MenuItem(("Undo " + undo.GetUndoName()).c_str(), "Ctrl+Z", false, undo.CanUndo())) {
				Context->Undo.Undo(*Context);
			}
			if (ImGui::MenuItem(("Redo " + undo.GetRedoName()).c_str(), "Ctrl+Y", false, undo.CanRedo())) {
				Context->Undo.Redo(*Context);
			}
			ImGui::EndMenu();
		}

		DrawWindowsMenu();

		ImGui::EndMainMenuBar();
	}

	void EditorApp::DrawWindowsMenu()
	{
		if (!ImGui::BeginMenu("Windows")) {
			return;
		}

		for (const auto& panel : Panels) {
			bool bOpen = panel->IsOpen();
			if (ImGui::MenuItem(panel->GetTitle().c_str(), nullptr, &bOpen)) {
				if (bOpen) {
					panel->Focus();
				} else {
					panel->SetOpen(false);
				}
			}
		}

		if (!AssetEditors.empty()) {
			ImGui::SeparatorText("Asset editors");
			for (const auto& editor : AssetEditors) {
				if (ImGui::MenuItem(editor->GetTitle().c_str())) {
					editor->Focus();
				}
			}
		}

		ImGui::Separator();
		if (ImGui::MenuItem("Reset Layout")) {
			bResetLayoutRequested = true;
		}
		ImGui::EndMenu();
	}

	void EditorApp::DrawProjectEditor()
	{
		DrawMainMenuBar();

		std::filesystem::path chosenProject;
		if (OpenProjectDialog.PollResult(chosenProject)) {
			LaunchEditor(chosenProject);
		}

		// The scene that was open last time (EditorLayout.ini is read during the first frame).
		if (!bStartupSceneHandled) {
			bStartupSceneHandled = true;
			std::error_code error;
			if (!StartupScene.empty() && std::filesystem::exists(StartupScene, error)) {
				OpenScene(StartupScene);
			}
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) {
			SaveScene();
		}
		// A focused text field keeps its own Ctrl+Z (global routes have the lowest priority).
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) {
			Context->Undo.Undo(*Context);
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) {
			Context->Undo.Redo(*Context);
		}

		const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport();
		if (bResetLayoutRequested) {
			BuildDefaultLayout(dockspaceId);
			bResetLayoutRequested = false;
		}

		// The editor doesn't run game systems; it only keeps world transforms current for drawing and picking.
		UpdateWorldTransforms(Context->EditorScene.GetWorld());

		for (const auto& panel : Panels) {
			if (panel->IsOpen()) {
				panel->Draw();
			}
		}
		for (const auto& editor : AssetEditors) {
			if (editor->IsOpen()) {
				editor->Draw();
			}
		}
		// A closed asset editor is gone for good (it releases its asset); reopening creates a new one.
		std::erase_if(AssetEditors, [](const std::unique_ptr<AssetEditor>& editor) { return !editor->IsOpen(); });

		DrawSceneDialogs();
		UpdateWindowTitle();
	}

	// -----------------------
	// Scene files
	// -----------------------

	void EditorApp::NewScene()
	{
		Context->EditorScene.Clear();
		Context->ScenePath.clear();
		Context->CurrentSelection.Clear();
		Context->SavedSceneRevision = Context->SceneRevision;
		Context->Undo.Reset(*Context);
		ImGui::MarkIniSettingsDirty();
	}

	bool EditorApp::OpenScene(const std::filesystem::path& sceneFile)
	{
		std::vector<std::string> warnings;
		std::string error;
		Context->CurrentSelection.Clear();
		if (!SceneSerializer::LoadFromFile(Context->EditorScene, sceneFile, warnings, error)) {
			SceneError = "Could not open " + PathToUtf8(sceneFile) + ":\n" + error;
			bRequestSceneErrorPopup = true;
			return false;
		}
		for (const std::string& warning : warnings) {
			SDL_Log("%s: %s", PathToUtf8(sceneFile.filename()).c_str(), warning.c_str());
		}
		Context->ScenePath = sceneFile;
		Context->SavedSceneRevision = Context->SceneRevision;
		Context->Undo.Reset(*Context);
		ImGui::MarkIniSettingsDirty();
		return true;
	}

	void EditorApp::SaveScene()
	{
		if (Context->ScenePath.empty()) {
			if (!SaveSceneDialog.IsOpen()) {
				SaveSceneDialog.SaveFile(Window, "FlexEngine Scene", "scene", Project::GetActive()->GetContentDirectory());
			}
			return;
		}
		SaveSceneAs(Context->ScenePath);
	}

	bool EditorApp::SaveSceneAs(const std::filesystem::path& sceneFile)
	{
		std::error_code fileError;
		const bool bNewFile = !std::filesystem::exists(sceneFile, fileError);
		std::string error;
		if (!SceneSerializer::SaveToFile(Context->EditorScene, sceneFile, error)) {
			SceneError = "Could not save " + PathToUtf8(sceneFile) + ":\n" + error;
			bRequestSceneErrorPopup = true;
			return false;
		}
		Context->ScenePath = sceneFile;
		Context->SavedSceneRevision = Context->SceneRevision;
		ImGui::MarkIniSettingsDirty();
		if (bNewFile) {
			RefreshAssets(); // gives the new file its .meta and shows it in Content
		}
		return true;
	}

	void EditorApp::RunAfterSavePrompt(std::function<void()> action)
	{
		if (!Context->IsSceneDirty()) {
			action();
			return;
		}
		ActionAfterSave = std::move(action);
		bRequestSavePrompt = true;
	}

	void EditorApp::DrawSceneDialogs()
	{
		std::filesystem::path chosen;
		if (OpenSceneDialog.PollResult(chosen)) {
			RunAfterSavePrompt([this, chosen]() { OpenScene(chosen); });
		}
		if (SaveSceneDialog.PollResult(chosen)) {
			if (chosen.extension() != SceneSerializer::FileExtension) {
				chosen += SceneSerializer::FileExtension;
			}
			if (SaveSceneAs(chosen) && bContinueAfterSaveAs && ActionAfterSave) {
				ActionAfterSave();
			}
			bContinueAfterSaveAs = false;
			ActionAfterSave = nullptr;
		} else if (bContinueAfterSaveAs && !SaveSceneDialog.IsOpen()) {
			bContinueAfterSaveAs = false; // the dialog was cancelled: so is the action
			ActionAfterSave = nullptr;
		}

		if (bRequestSavePrompt) {
			ImGui::OpenPopup(SavePromptPopup);
			bRequestSavePrompt = false;
		}
		if (ImGui::BeginPopupModal(SavePromptPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			const std::string sceneName = Context->ScenePath.empty() ? std::string("Untitled") : PathToUtf8(Context->ScenePath.filename());
			ImGui::Text("Save changes to %s?", sceneName.c_str());
			if (ImGui::Button("Save")) {
				if (Context->ScenePath.empty()) {
					bContinueAfterSaveAs = true;
					SaveScene(); // opens "Save As"; the action runs once a file was chosen
				} else if (SaveSceneAs(Context->ScenePath) && ActionAfterSave) {
					ActionAfterSave();
					ActionAfterSave = nullptr;
				}
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Don't Save")) {
				if (ActionAfterSave) {
					ActionAfterSave();
				}
				ActionAfterSave = nullptr;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				ActionAfterSave = nullptr;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (bRequestSceneErrorPopup) {
			ImGui::OpenPopup(SceneErrorPopup);
			bRequestSceneErrorPopup = false;
		}
		if (ImGui::BeginPopupModal(SceneErrorPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::TextUnformatted(SceneError.c_str());
			if (ImGui::Button("OK")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void EditorApp::UpdateWindowTitle()
	{
		const std::string sceneName = Context->ScenePath.empty() ? std::string("Untitled") : PathToUtf8(Context->ScenePath.stem());
		const std::string title = "FlexEngine - " + Project::GetActive()->GetName() + " - " + sceneName
			+ (Context->IsSceneDirty() ? "*" : "") + " (" + Engine::GetBuildConfigName() + ")";
		if (title != WindowTitle) {
			WindowTitle = title;
			SDL_SetWindowTitle(Window, WindowTitle.c_str());
		}
	}

	void EditorApp::OpenAsset(Uid assetId)
	{
		const AssetInfo* asset = Assets.Find(assetId);
		if (!asset) {
			return;
		}
		if (auto opener = AssetOpeners.find(asset->Type); opener != AssetOpeners.end()) {
			opener->second(*asset);
		}
	}

	void EditorApp::OpenAssetEditor(Uid editedAssetId, Uid subAssetId, const std::function<std::unique_ptr<AssetEditor>()>& create)
	{
		for (const auto& editor : AssetEditors) {
			if (editor->GetAssetId() == editedAssetId) {
				editor->Focus();
				editor->OnReopen(subAssetId);
				return;
			}
		}
		AssetEditors.push_back(create());
		AssetEditors.back()->Focus();
	}

	void EditorApp::RefreshAssets()
	{
		LastAssetRefresh = Assets.Refresh(Project::GetActive()->GetContentDirectory());
		Context->Thumbnails.Clear();
		ResetSpriteCaches(Context->EditorScene.GetWorld()); // reload changed assets on the next draw
		++Context->AssetRevision;
		AssetManager::ClearFailures();

		SDL_Log("Assets: %d found, %d .meta created, %d duplicate ids fixed, %d orphaned .meta, %d errors",
			LastAssetRefresh.AssetCount, LastAssetRefresh.MetaCreated, LastAssetRefresh.DuplicatesFixed, LastAssetRefresh.OrphanMetas, LastAssetRefresh.Errors);
		for (const std::string& message : LastAssetRefresh.Messages) {
			SDL_Log("  %s", message.c_str());
		}
	}
}
