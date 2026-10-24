#include "Editor/EditorApp.h"
#include "Editor/ContentBrowser.h"
#include "Editor/EditorAssetSource.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorPreferences.h"
#include "Editor/EditorWindow.h"
#include "Editor/EntitiesWindow.h"
#include "Editor/HierarchyWindow.h"
#include "Editor/InputMapEditor.h"
#include "Editor/InputReceiverInspector.h"
#include "Editor/InspectorArchive.h"
#include "Editor/InspectorWindow.h"
#include "Editor/OutputWindow.h"
#include "Editor/ProjectScripts.h"
#include "Editor/ProjectBrowser.h"
#include "Editor/ProjectBuilder.h"
#include "Editor/TextureEditor.h"
#include "Editor/TextureImporter.h"
#include "Editor/ViewportWindow.h"
#include "Core/Assets/AssetManager.h"
#include "Core/Engine.h"
#include "Core/Game/GameModule.h"
#include "Core/Graphics/SpriteRenderer.h"
#include "Core/Input/Input.h"
#include "Core/Input/InputMap.h"
#include "Core/World/GameObject.h"
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
#include <cstdio>
#include <cstring>
#include <vector>

namespace Flex
{
	namespace
	{
		// Project name, folder, game module, start scene and frame rate.
		class ProjectWindow final : public EditorWindow
		{
		public:
			explicit ProjectWindow(EditorContext& context) : EditorWindow("Project"), Context{ context } {}

			void Draw() override
			{
				if (BeginWindow()) {
					Project* project = Project::GetActive();
					ImGui::Text("Name: %s", project->GetName().c_str());
					ImGui::Text("Folder: %s", PathToUtf8(project->GetRootDirectory()).c_str());
					ImGui::Text("Created with engine: %s", project->GetEngineVersion().c_str());

					ImGui::SeparatorText("Game");
					ImGui::TextWrapped("Module: %s", Context.GameModuleStatus.c_str());
					if (ImGui::BeginTable("ProjectSettings", 2, ImGuiTableFlags_SizingStretchProp)) {
						ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 90.0f);
						ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
						InspectorArchive archive(Context);
						Uid startScene = project->GetStartScene();
						archive.Field("Start Scene", startScene, { SceneSerializer::AssetTypeName });
						ImGui::EndTable();
						if (archive.WasChanged()) {
							project->SetStartScene(startScene);
							project->Save();
						}
					}

					ImGui::SeparatorText("Physics");
					ImGui::TextDisabled("Used from the next Play.");
					if (ImGui::BeginTable("PhysicsSettings", 2, ImGuiTableFlags_SizingStretchProp)) {
						ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
						ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
						InspectorArchive archive(Context);
						PhysicsSettings physics = project->GetPhysicsSettings();
						physics.Serialize(archive);
						ImGui::EndTable();
						if (archive.WasChanged()) {
							project->SetPhysicsSettings(physics);
						}
						if (archive.WasEditFinished()) {
							project->Save();
						}
					}

					ImGui::Separator();
					ImGui::Text("FlexEngine %s (%s)", Engine::GetVersion(), Engine::GetBuildConfigName());
					ImGui::Text("%.1f FPS (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
				}
				EndWindow();
			}

		private:
			EditorContext& Context;
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
			Assets.RegisterType(InputMap::TypeName, { InputMap::FileExtension });
			Assets.SetOwnerCacheFile(project->GetSavedDirectory() / "AssetOwners.cache");
			AssetSourceForEditor = std::make_unique<EditorAssetSource>(Assets);
			AssetManager::SetSource(AssetSourceForEditor.get());

			Context = std::make_unique<EditorContext>(Assets);
			Context->OpenAsset = [this](Uid assetId) { OpenAsset(assetId); };
			Context->RefreshAssets = [this]() { RefreshAssets(); };
			Context->TogglePlay = [this]() { Context->bIsPlaying ? StopPlay() : StartPlay(); };
			Context->NewScript = [this]() { bRequestNewScriptPopup = true; };
			Context->RecompileScripts = [this]() { RecompileScripts(); };
			Context->Output.InstallSdlLogHook(); // from here on, engine log lines also go to the Output window
			ProjectScripts::UpdateEngineLocation(*project);
			LoadGameModule(); // before scenes load: the module's components must be registered
			CreateProjectWindows();
			Plugins.LoadAll(*this, *project); // after the editor's own types and windows, before the first asset scan
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
		Panels.push_back(std::make_unique<EntitiesWindow>(*Context));
		Panels.push_back(std::make_unique<InspectorWindow>(*Context));
		RegisterInputReceiverInspector();
		Panels.push_back(std::make_unique<ProjectWindow>(*Context));
		Panels.push_back(std::make_unique<PluginsWindow>(Plugins));
		Panels.push_back(std::make_unique<OutputWindow>(*Context));
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
		AssetOpeners[InputMap::TypeName] = [this](const AssetInfo& asset) {
			const Uid inputMapId = asset.Id;
			OpenAssetEditor(inputMapId, {}, [this, inputMapId]() { return std::make_unique<InputMapEditor>(*Context, inputMapId); });
		};
		AddContentCreateItem("Input Map...", [](const std::filesystem::path& folder) { InputMapEditor::OpenNewInputMapDialog(folder); });
		AddMenuItem("Assets", "New Input Map...", "", [this]() { InputMapEditor::OpenNewInputMapDialog(Assets.GetContentDirectory()); }, {});
		AddDrawCallback([this]() { InputMapEditor::DrawNewInputMapDialog(*Context); });
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
		ImGui::DockBuilderDockWindow("Entities", right);
		ImGui::DockBuilderDockWindow("Content", bottom);
		ImGui::DockBuilderDockWindow("Project", bottom);
		ImGui::DockBuilderDockWindow("Plugins", bottom);
		ImGui::DockBuilderDockWindow("Output", bottom);
		ImGui::DockBuilderDockWindow("Viewport", center);
		ImGui::DockBuilderFinish(dockspaceId);

		for (const auto& panel : Panels) {
			panel->SetOpen(true);
		}
		FrontTabsCountdown = 2; // once the windows exist, show these tabs
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
			// The game's input: gamepads always (so player slots stay right), keyboard and mouse buttons only
			// while playing with the Viewport focused (as of the last frame).
			Input::BeginFrame();
			const bool bGameInput = Context && Context->bGameHasInputFocus;
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				ImGui_ImplSDL3_ProcessEvent(&event);
				if (bGameInput || !Input::IsKeyboardOrMouseEvent(event)) {
					Input::ProcessEvent(event);
				}
				if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
					Input::ReleaseKeyboardMouse();
				}
				if (event.type == SDL_EVENT_QUIT) {
					RequestQuit();
				}
				if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(Window)) {
					RequestQuit();
				}
			}

			Input::Update();

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
			Renderer::EndFrame();
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
		PlayKeepAlive.clear();
		AssetEditors.clear();
		Panels.clear();
		Plugins.UnloadAll(); // after their windows, before the assets are released
		if (Context) {
			Context->Output.UninstallSdlLogHook();
		}
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
			StopPlay();
			RunAfterSavePrompt([this]() { bIsRunning = false; });
		} else {
			bIsRunning = false;
		}
	}

	bool EditorApp::LaunchRuntime(const std::filesystem::path& sceneFile)
	{
		const char* basePath = SDL_GetBasePath();
		if (!basePath || !Project::GetActive()) {
			return false;
		}
		const std::string runtimeExe = std::string(basePath) + "FlexRuntime.exe";
		const std::string projectArgument = PathToUtf8(Project::GetActive()->GetProjectFile());
		const std::string sceneArgument = PathToUtf8(sceneFile);
		const char* arguments[] = { runtimeExe.c_str(), "--project", projectArgument.c_str(), "--scene", sceneArgument.c_str(), nullptr };
		SDL_Process* process = SDL_CreateProcess(arguments, false);
		if (!process) {
			SDL_Log("Could not start %s: %s", runtimeExe.c_str(), SDL_GetError());
			return false;
		}
		SDL_DestroyProcess(process); // only frees SDL's handle
		return true;
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
			if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, !Context->bIsPlaying)) {
				SaveScene();
			}
			if (ImGui::MenuItem("Save Scene As...", nullptr, false, !SaveSceneDialog.IsOpen() && !Context->bIsPlaying)) {
				SaveSceneDialog.SaveFile(Window, "FlexEngine Scene", "scene", Project::GetActive()->GetContentDirectory());
			}
			ImGui::Separator();
			if (ImGui::MenuItem(Context->bIsPlaying ? "Stop" : "Play", "F5")) {
				Context->TogglePlay();
			}
			if (ImGui::MenuItem(BuildJob.IsRunning() ? "Building Project..." : "Build Project...", nullptr, false, !BuildJob.IsRunning() && !ScriptJob.IsRunning())) {
				bRequestBuildPopup = true;
			}
			if (ImGui::MenuItem("Run Standalone", nullptr, false, !Context->ScenePath.empty())) {
				LaunchRuntime(Context->ScenePath);
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip(Context->ScenePath.empty() ? "Save the scene first." : "Runs the saved scene file in FlexRuntime.");
			}
			ImGui::Separator();
			if (ImGui::MenuItem("New Project...")) {
				LaunchEditor({});
			}
			if (ImGui::MenuItem("Open Project...", nullptr, false, !OpenProjectDialog.IsOpen())) {
				OpenProjectDialog.OpenFile(Window, "FlexEngine Project", "flexproj", Project::GetActive()->GetRootDirectory().parent_path());
			}
			DrawPluginMenuItems("File", true);
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) {
				RequestQuit();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			const SceneUndo& undo = Context->Undo;
			if (ImGui::MenuItem(("Undo " + undo.GetUndoName()).c_str(), "Ctrl+Z", false, undo.CanUndo() && !Context->bIsPlaying)) {
				Context->Undo.Undo(*Context);
			}
			if (ImGui::MenuItem(("Redo " + undo.GetRedoName()).c_str(), "Ctrl+Y", false, undo.CanRedo() && !Context->bIsPlaying)) {
				Context->Undo.Redo(*Context);
			}
			DrawPluginMenuItems("Edit", true);
			ImGui::EndMenu();
		}

		DrawScriptsMenu();

		// Menus that only plugins add, in the order they were first used.
		std::vector<std::string> pluginMenus;
		for (const PluginMenuItem& item : PluginMenuItems) {
			const bool bBuiltIn = item.Menu == "File" || item.Menu == "Edit" || item.Menu == "Scripts" || item.Menu == "Windows";
			if (!bBuiltIn && std::find(pluginMenus.begin(), pluginMenus.end(), item.Menu) == pluginMenus.end()) {
				pluginMenus.push_back(item.Menu);
			}
		}
		for (const std::string& menu : pluginMenus) {
			if (ImGui::BeginMenu(menu.c_str())) {
				DrawPluginMenuItems(menu, false);
				ImGui::EndMenu();
			}
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

		DrawPluginMenuItems("Windows", true);
		ImGui::Separator();
		if (ImGui::MenuItem("Reset Layout")) {
			bResetLayoutRequested = true;
		}
		ImGui::EndMenu();
	}

	void EditorApp::DrawPluginMenuItems(const std::string& menu, bool bSeparate)
	{
		for (const PluginMenuItem& item : PluginMenuItems) {
			if (item.Menu != menu) {
				continue;
			}
			if (bSeparate) {
				ImGui::Separator();
				bSeparate = false;
			}
			const bool bEnabled = !item.IsEnabled || item.IsEnabled();
			if (ImGui::MenuItem(item.Label.c_str(), item.Shortcut.empty() ? nullptr : item.Shortcut.c_str(), false, bEnabled)) {
				item.Action();
			}
		}
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
		// While an Input Map editor listens for an input to bind, the editor's own shortcuts are off.
		const bool bShortcuts = !InputMapEditor::IsListening();
		if (bShortcuts && ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal)) {
			Context->TogglePlay();
		}
		if (bShortcuts && ImGui::Shortcut(ImGuiKey_F7, ImGuiInputFlags_RouteGlobal)) {
			RecompileScripts();
		}
		bool bBuildSucceeded = false;
		if (ScriptJob.Poll(bBuildSucceeded)) {
			OnScriptsBuilt(bBuildSucceeded);
		}
		if (BuildJob.Poll(bBuildSucceeded)) {
			Context->ScriptJobStatus.clear();
			if (bBuildSucceeded && bOpenBuildFolder) {
				SDL_OpenURL(("file:///" + PathToGenericUtf8(BuildTarget)).c_str());
			}
		}
		// While playing, saving and undo are off: Stop discards the changes anyway.
		if (!Context->bIsPlaying && bShortcuts) {
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
		}

		const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport();
		if (bResetLayoutRequested) {
			BuildDefaultLayout(dockspaceId);
			bResetLayoutRequested = false;
		}

		// Editing runs no game systems, only keeps world transforms current for drawing and picking.
		if (Context->bIsPlaying) {
			Context->Game.Tick(Context->EditorScene, ImGui::GetIO().DeltaTime);
		} else {
			UpdateWorldTransforms(Context->EditorScene.GetWorld());
		}

		// The Viewport sets it again while it's focused during Play; losing it releases held keys.
		const bool bHadGameInput = Context->bGameHasInputFocus;
		Context->bGameHasInputFocus = false;

		// By index: a window may open another one (e.g. a plugin's asset editor) while it draws.
		for (size_t index = 0; index < Panels.size(); ++index) {
			if (Panels[index]->IsOpen()) {
				Panels[index]->Draw();
			}
		}
		for (size_t index = 0; index < AssetEditors.size(); ++index) {
			if (AssetEditors[index]->IsOpen()) {
				AssetEditors[index]->Draw();
			}
		}
		if (bHadGameInput && !Context->bGameHasInputFocus) {
			Input::ReleaseKeyboardMouse();
		}
		for (const std::function<void()>& draw : PluginDrawCallbacks) {
			draw();
		}
		if (FrontTabsCountdown > 0 && --FrontTabsCountdown == 0) {
			for (const char* title : { "Inspector", "Content" }) {
				if (EditorWindow* panel = FindPanel(title)) {
					panel->Focus();
				}
			}
		}

		// A closed asset editor is gone for good (it releases its asset); reopening creates a new one.
		std::erase_if(AssetEditors, [](const std::unique_ptr<AssetEditor>& editor) { return !editor->IsOpen(); });

		DrawSceneDialogs();
		DrawNewScriptDialog();
		DrawBuildProjectDialog();
		UpdateWindowTitle();
	}

	// -----------------------
	// Scene files
	// -----------------------

	void EditorApp::NewScene()
	{
		StopPlay();
		Context->EditorScene.Clear();
		Context->ScenePath.clear();
		Context->CurrentSelection.Clear();
		Context->SavedSceneRevision = Context->SceneRevision;
		Context->Undo.Reset(*Context);
		ImGui::MarkIniSettingsDirty();
	}

	bool EditorApp::OpenScene(const std::filesystem::path& sceneFile)
	{
		StopPlay();
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

	// -----------------------
	// Scripts
	// -----------------------

	void EditorApp::DrawScriptsMenu()
	{
		if (!ImGui::BeginMenu("Scripts")) {
			return;
		}
		Project* project = Project::GetActive();
		if (ImGui::MenuItem("New Script...")) {
			bRequestNewScriptPopup = true;
		}
		if (ImGui::MenuItem(ScriptJob.IsRunning() ? "Recompiling..." : "Recompile", "F7", false, !ScriptJob.IsRunning())) {
			RecompileScripts();
		}
		ImGui::Separator();
		std::error_code error;
		const std::filesystem::path solution = ProjectScripts::GetSolutionFile(*project);
		const std::filesystem::path moduleProject = ProjectScripts::GetModuleProjectFile(*project);
		const std::filesystem::path toOpen = std::filesystem::exists(solution, error) ? solution : moduleProject;
		if (ImGui::MenuItem("Open in Visual Studio", nullptr, false, std::filesystem::exists(toOpen, error))) {
			SDL_OpenURL(("file:///" + PathToGenericUtf8(toOpen)).c_str());
		}
		if (ImGui::MenuItem("Open Source Folder", nullptr, false, std::filesystem::exists(ProjectScripts::GetSourceDirectory(*project), error))) {
			SDL_OpenURL(("file:///" + PathToGenericUtf8(ProjectScripts::GetSourceDirectory(*project))).c_str());
		}
		DrawPluginMenuItems("Scripts", true);
		ImGui::EndMenu();
	}

	void EditorApp::DrawNewScriptDialog()
	{
		constexpr const char* PopupName = "New Script";
		if (bRequestNewScriptPopup) {
			bRequestNewScriptPopup = false;
			NewScriptName[0] = '\0';
			NewScriptError.clear();
			ImGui::OpenPopup(PopupName);
		}
		if (!ImGui::BeginPopupModal(PopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			return;
		}
		Project* project = Project::GetActive();
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}
		const bool bEnter = ImGui::InputTextWithHint("Name", "e.g. Health", NewScriptName, sizeof(NewScriptName), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::RadioButton("Component", &NewScriptTemplate, 0);
		ImGui::SameLine();
		ImGui::RadioButton("System", &NewScriptTemplate, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Component + System", &NewScriptTemplate, 2);
		const std::string name = NewScriptName;
		const char* files = NewScriptTemplate == 1 ? ".cpp" : ".h + .cpp";
		ImGui::TextDisabled("Source/%s %s, namespace %s", name.empty() ? "<Name>" : name.c_str(), files, project->GetName().c_str());
		ImGui::Checkbox("Open in code editor", &bOpenNewScript);
		if (!NewScriptError.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", NewScriptError.c_str());
		}

		if (ImGui::Button("Create") || bEnter) {
			std::vector<std::filesystem::path> created;
			if (ProjectScripts::CreateScript(*project, name, static_cast<ProjectScripts::ScriptTemplate>(NewScriptTemplate), created, NewScriptError)) {
				for (const std::filesystem::path& file : created) {
					Context->Output.Add(OutputLog::Level::Success, "Created " + PathToUtf8(file));
					if (bOpenNewScript && file.extension() == ".cpp") {
						SDL_OpenURL(("file:///" + PathToGenericUtf8(file)).c_str());
					}
				}
				Context->Output.Add(OutputLog::Level::Info, "Press Recompile (F7) to build it into " + project->GetName() + ".dll.");
				RefreshAssets(); // the Content window lists the new files
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void EditorApp::DrawBuildProjectDialog()
	{
		constexpr const char* PopupName = "Build Project";
		Project* project = Project::GetActive();
		if (bRequestBuildPopup) {
			bRequestBuildPopup = false;
			BuildError.clear();
			if (BuildOutput[0] == '\0') {
				std::snprintf(BuildOutput, sizeof(BuildOutput), "%s", PathToUtf8(ProjectBuilder::GetDefaultOutputDirectory(*project)).c_str());
			}
			ImGui::OpenPopup(PopupName);
		}
		std::filesystem::path chosenFolder;
		if (BuildFolderDialog.PollResult(chosenFolder)) {
			std::snprintf(BuildOutput, sizeof(BuildOutput), "%s", PathToUtf8(chosenFolder).c_str());
		}
		if (!ImGui::BeginPopupModal(PopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			return;
		}

		ImGui::TextUnformatted("A standalone folder that runs the game without the editor.");
		const char* configurations[] = { "Development", "Shipping" };
		ImGui::Combo("Configuration", &BuildConfiguration, configurations, 2);
		ImGui::TextDisabled(BuildConfiguration == 0 ? "Optimised, with debug tools and a console window." : "Optimised, no debug tools, no console window.");
		ImGui::SetNextItemWidth(420.0f);
		ImGui::InputText("##Output", BuildOutput, sizeof(BuildOutput));
		ImGui::SameLine();
		if (ImGui::Button("Browse...") && !BuildFolderDialog.IsOpen()) {
			BuildFolderDialog.OpenFolder(Window, Utf8ToPath(BuildOutput));
		}
		ImGui::TextDisabled("Output: %s/%s/%s.exe", BuildOutput, project->GetName().c_str(), project->GetName().c_str());
		ImGui::Checkbox("Open the folder when done", &bOpenBuildFolder);
		if (!BuildError.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", BuildError.c_str());
		}

		if (ImGui::Button("Build")) {
			const std::string configuration = configurations[BuildConfiguration];
			if (ProjectBuilder::CheckEngineBinaries(configuration, BuildError) && !BuildJob.IsRunning() && !ScriptJob.IsRunning()) {
				StopPlay();
				const std::filesystem::path outputDirectory = Utf8ToPath(BuildOutput);
				BuildTarget = outputDirectory / Utf8ToPath(project->GetName());
				Context->ScriptJobStatus = "Building " + project->GetName() + " (" + configuration + ")...";
				if (EditorWindow* output = FindPanel("Output")) {
					output->Focus();
				}
				OutputLog& log = Context->Output;
				const std::vector<std::string> skippedExtensions = Assets.GetEditorOnlyExtensions();
				std::vector<BuildFile> extraFiles;
				for (const auto& files : PluginBuildFiles) {
					for (BuildFile& file : files(*project, configuration)) {
						extraFiles.push_back(std::move(file));
					}
				}
				BuildJob.Start([project, configuration, outputDirectory, skippedExtensions, extraFiles, &log]() {
					return ProjectBuilder::Build(*project, configuration, outputDirectory, skippedExtensions, extraFiles, log);
				});
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void EditorApp::RecompileScripts()
	{
		if (ScriptJob.IsRunning() || BuildJob.IsRunning()) {
			return;
		}
		StopPlay();
		Project* project = Project::GetActive();
		std::string error;
		std::error_code fileError;
		if (std::filesystem::exists(ProjectScripts::GetSourceDirectory(*project), fileError) && !ProjectScripts::EnsureProjectFiles(*project, error)) {
			Context->Output.Add(OutputLog::Level::Error, error);
			return;
		}
		if (EditorWindow* output = FindPanel("Output")) {
			output->Focus();
		}
		const std::string configuration = Engine::GetBuildConfigName();
		Context->ScriptJobStatus = "Building " + project->GetName() + ".dll (" + configuration + ")...";
		OutputLog& log = Context->Output;
		ScriptJob.Start([project, configuration, &log]() { return ProjectScripts::BuildModule(*project, configuration, log); });
	}

	void EditorApp::OnScriptsBuilt(bool bSucceeded)
	{
		Context->ScriptJobStatus.clear();
		if (bSucceeded) {
			ReloadGameModule();
		}
	}

	// -----------------------
	// Game module and Play mode
	// -----------------------

	// Loads a copy from Saved/ModuleCache, so Visual Studio (or Recompile) can rebuild the
	// original while the editor runs. Recompile then hot-reloads it (ReloadGameModule).
	void EditorApp::LoadGameModule()
	{
		Project* project = Project::GetActive();
		const std::filesystem::path moduleFile = GameModule::FindModuleFile(*project);
		if (moduleFile.empty()) {
			Context->GameModuleStatus = "none (" + project->GetName() + ".dll not built)";
			return;
		}

		std::error_code error;
		// A module built against older engine headers has the wrong struct layouts: give a hint.
		if (const char* basePath = SDL_GetBasePath()) {
			const std::filesystem::path engineCore = Utf8ToPath(basePath) / "FlexCore.dll";
			std::error_code timeError;
			if (std::filesystem::last_write_time(moduleFile, timeError) < std::filesystem::last_write_time(engineCore, timeError)) {
				SDL_Log("%s was built before the current FlexCore.dll. If engine headers changed since, press Recompile (F7).", PathToUtf8(moduleFile.filename()).c_str());
			}
		}

		const std::filesystem::path cacheDirectory = project->GetSavedDirectory() / "ModuleCache";
		std::filesystem::create_directories(cacheDirectory, error);
		for (const auto& old : std::filesystem::directory_iterator(cacheDirectory, error)) {
			std::error_code removeError;
			std::filesystem::remove(old.path(), removeError); // copies still loaded (by other editors) stay
		}
		const std::string copyName = project->GetName() + "-" + std::to_string(SDL_GetCurrentThreadID()) + "-" + std::to_string(++ModuleLoadCount) + ".dll";
		const std::filesystem::path copy = cacheDirectory / Utf8ToPath(copyName);
		const bool bCopied = std::filesystem::copy_file(moduleFile, copy, std::filesystem::copy_options::overwrite_existing, error);

		std::string loadError;
		if (GameModule::Load(bCopied ? copy : moduleFile, loadError)) {
			Context->GameModuleStatus = PathToUtf8(moduleFile.filename()) + " (Recompile reloads it)";
		} else {
			Context->GameModuleStatus = "failed: " + loadError;
			SDL_Log("%s", loadError.c_str());
		}
	}

	void EditorApp::ReloadGameModule()
	{
		StopPlay();
		const uint64_t start = SDL_GetPerformanceCounter();

		// 1. The scene as text (every component with a Serialize, including the module's).
		TextDocument sceneText;
		SceneSerializer::Save(Context->EditorScene, sceneText);
		Uid selected;
		if (const GameObjectInfo* info = Context->EditorScene.GetWorld().GetComponent<GameObjectInfo>(Context->CurrentSelection.GetEntity())) {
			selected = info->Id;
		}
		std::vector<std::shared_ptr<Asset>> keepAlive; // don't unload textures just to reload them
		Context->EditorScene.GetWorld().Each<const SpriteRenderer>([&](const SpriteRenderer& renderer) {
			if (renderer.Loaded && (keepAlive.empty() || keepAlive.back() != renderer.Loaded)) {
				keepAlive.push_back(renderer.Loaded);
			}
		});

		// 2. No component of the old module may survive its unload.
		Context->EditorScene.Clear();
		Context->Game = GameInstance{}; // drops system pointers into the old module
		size_t componentTypes = 0;
		size_t systems = 0;
		GameModule::Unload(componentTypes, systems);

		// 3. The rebuilt module registers its components and systems again (layouts may change).
		LoadGameModule();

		// 4. The scene back: fields that still exist keep their values; components the module
		//    no longer has are kept as unknown sections, so nothing is lost.
		std::vector<std::string> warnings;
		std::string error;
		SceneSerializer::Load(Context->EditorScene, sceneText, warnings, error);
		for (const std::string& warning : warnings) {
			Context->Output.Add(OutputLog::Level::Warning, warning);
		}
		if (Context->CurrentSelection.GetKind() == Selection::Kind::Entity) {
			Context->CurrentSelection.SelectEntity(Context->EditorScene.FindByUid(selected));
		}

		const double milliseconds = static_cast<double>(SDL_GetPerformanceCounter() - start) * 1000.0 / static_cast<double>(SDL_GetPerformanceFrequency());
		if (GameModule::IsLoaded()) {
			Context->Output.Add(OutputLog::Level::Success, "Game module reloaded in " + std::to_string(static_cast<int>(milliseconds)) + " ms ("
				+ std::to_string(componentTypes) + " component types and " + std::to_string(systems) + " systems replaced).");
		} else {
			Context->Output.Add(OutputLog::Level::Error, "The game module could not be loaded: " + Context->GameModuleStatus);
		}
	}

	void EditorApp::StartPlay()
	{
		if (Context->bIsPlaying) {
			return;
		}
		SceneSerializer::Save(Context->EditorScene, PlaySnapshot);
		PlayRevision = Context->SceneRevision;
		PlaySelection = Uid();
		if (const GameObjectInfo* info = Context->EditorScene.GetWorld().GetComponent<GameObjectInfo>(Context->CurrentSelection.GetEntity())) {
			PlaySelection = info->Id;
		}
		Context->bIsPlaying = true;
		Context->bGameView = true;
		Context->Game.Begin(Context->EditorScene);
	}

	void EditorApp::StopPlay()
	{
		if (!Context || !Context->bIsPlaying) {
			return;
		}
		// Keep the scene's assets loaded while it's rebuilt, so Stop doesn't reload every texture.
		PlayKeepAlive.clear();
		Context->EditorScene.GetWorld().Each<const SpriteRenderer>([&](const SpriteRenderer& renderer) {
			if (renderer.Loaded && (PlayKeepAlive.empty() || PlayKeepAlive.back() != renderer.Loaded)) {
				PlayKeepAlive.push_back(renderer.Loaded);
			}
		});

		std::vector<std::string> warnings;
		std::string error;
		const bool bAssetSelected = Context->CurrentSelection.GetKind() == Selection::Kind::Asset;
		SceneSerializer::Load(Context->EditorScene, PlaySnapshot, warnings, error);
		Context->bIsPlaying = false;
		Context->bGameView = false;
		Context->bGameHasInputFocus = false;
		Input::ReleaseKeyboardMouse();
		Context->SceneRevision = PlayRevision;
		if (!bAssetSelected) {
			Context->CurrentSelection.SelectEntity(Context->EditorScene.FindByUid(PlaySelection));
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

	// -----------------------
	// Plugin host
	// -----------------------

	void EditorApp::RegisterAssetType(const std::string& typeName, const std::vector<std::string>& extensions,
		AssetDatabase::MetaInitializer initializeMeta, AssetTypeFlags flags)
	{
		Assets.RegisterType(typeName, extensions, std::move(initializeMeta), flags);
	}

	void EditorApp::RegisterAssetOpener(const std::string& typeName, std::function<void(const AssetInfo&)> open)
	{
		AssetOpeners[typeName] = std::move(open);
	}

	void EditorApp::AddPanel(std::unique_ptr<EditorWindow> panel)
	{
		Panels.push_back(std::move(panel));
	}

	void EditorApp::AddMenuItem(const std::string& menu, const std::string& label, const std::string& shortcut,
		std::function<void()> action, std::function<bool()> isEnabled)
	{
		PluginMenuItems.push_back(PluginMenuItem{ menu, label, shortcut, std::move(action), std::move(isEnabled) });
	}

	void EditorApp::AddContentCreateItem(const std::string& label, std::function<void(const std::filesystem::path& folder)> create)
	{
		Context->ContentCreateItems.push_back(ContentCreateItem{ label, std::move(create) });
	}

	void EditorApp::RegisterThumbnail(const std::string& typeName, std::function<std::shared_ptr<Texture>(const AssetInfo&)> makeThumbnail)
	{
		Context->Thumbnails.SetProvider(typeName, std::move(makeThumbnail));
	}

	void EditorApp::AddDrawCallback(std::function<void()> draw)
	{
		PluginDrawCallbacks.push_back(std::move(draw));
	}

	void EditorApp::AddBuildFiles(std::function<std::vector<BuildFile>(const Project& project, const std::string& configuration)> files)
	{
		PluginBuildFiles.push_back(std::move(files));
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
