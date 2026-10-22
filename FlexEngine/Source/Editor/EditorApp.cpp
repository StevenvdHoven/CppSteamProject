#include "Editor/EditorApp.h"
#include "Editor/EditorAssetSource.h"
#include "Editor/EditorPreferences.h"
#include "Editor/ProjectBrowser.h"
#include "Editor/TextureEditor.h"
#include "Editor/TextureImporter.h"
#include "Core/Assets/AssetManager.h"
#include "Core/Engine.h"
#include "Core/FileSystem.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Project.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <vector>

namespace Flex
{
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
			if (!CreateWindowAndImGui(title, 1600, 900, project->GetSavedDirectory() / "EditorLayout.ini")) {
				return false;
			}

			TextureImporter::Register(Assets);
			Assets.SetOwnerCacheFile(project->GetSavedDirectory() / "AssetOwners.cache");
			AssetSourceForEditor = std::make_unique<EditorAssetSource>(Assets);
			AssetManager::SetSource(AssetSourceForEditor.get());
			TextureEditorWindow = std::make_unique<TextureEditor>(Assets);
			TextureEditorWindow->OnSpritesSaved = [this]() { RefreshAssets(); };
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
					bIsRunning = false;
				}
				if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(Window)) {
					bIsRunning = false;
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

		// Release every texture before the renderer goes away.
		Thumbnails.clear();
		SpriteThumbnails.clear();
		TextureEditorWindow.reset();
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

		ImGui::EndMainMenuBar();
	}

	void EditorApp::DrawProjectEditor()
	{
		Project* project = Project::GetActive();

		DrawMainMenuBar();

		std::filesystem::path chosenProject;
		if (OpenProjectDialog.PollResult(chosenProject)) {
			LaunchEditor(chosenProject);
		}

		ImGui::DockSpaceOverViewport();

		ImGui::Begin("Project");
		ImGui::Text("Name: %s", project->GetName().c_str());
		ImGui::Text("Folder: %s", PathToUtf8(project->GetRootDirectory()).c_str());
		ImGui::Text("Created with engine: %s", project->GetEngineVersion().c_str());
		ImGui::Separator();
		ImGui::Text("FlexEngine %s (%s)", Engine::GetVersion(), Engine::GetBuildConfigName());
		ImGui::Text("%.1f FPS (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
		ImGui::End();

		DrawAssetsPanel();
		TextureEditorWindow->Draw();
	}

	void EditorApp::RefreshAssets()
	{
		LastAssetRefresh = Assets.Refresh(Project::GetActive()->GetContentDirectory());
		Thumbnails.clear();
		SpriteThumbnails.clear();
		AssetManager::ClearFailures();

		SDL_Log("Assets: %d found, %d .meta created, %d duplicate ids fixed, %d orphaned .meta, %d errors",
			LastAssetRefresh.AssetCount, LastAssetRefresh.MetaCreated, LastAssetRefresh.DuplicatesFixed, LastAssetRefresh.OrphanMetas, LastAssetRefresh.Errors);
		for (const std::string& message : LastAssetRefresh.Messages) {
			SDL_Log("  %s", message.c_str());
		}
	}

	// Temporary panel to see the asset database at work, until there's a real content browser.
	void EditorApp::DrawAssetsPanel()
	{
		ImGui::Begin("Assets");

		if (ImGui::Button("Refresh")) {
			RefreshAssets();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%d assets | %d .meta created | %d duplicates fixed | %d orphaned .meta | %d errors",
			LastAssetRefresh.AssetCount, LastAssetRefresh.MetaCreated, LastAssetRefresh.DuplicatesFixed, LastAssetRefresh.OrphanMetas, LastAssetRefresh.Errors);

		if (!LastAssetRefresh.Messages.empty() && ImGui::TreeNode("Messages")) {
			for (const std::string& message : LastAssetRefresh.Messages) {
				ImGui::TextWrapped("%s", message.c_str());
			}
			ImGui::TreePop();
		}

		// Proof that sharing works: one entry per loaded asset, no matter how many users.
		size_t loadedBytes = 0;
		const auto loadedAssets = AssetManager::GetLoadedAssets();
		for (const auto& loaded : loadedAssets) {
			loadedBytes += loaded->GetMemorySize();
		}
		ImGui::TextDisabled("Loaded: %d asset(s), %.2f MB | GPU textures alive: %d",
			static_cast<int>(loadedAssets.size()), loadedBytes / (1024.0 * 1024.0), Renderer::GetLiveTextureCount());

		const float thumbnailSize = 32.0f;
		if (ImGui::BeginTable("AssetTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, thumbnailSize);
			ImGui::TableSetupColumn("Path");
			ImGui::TableSetupColumn("Type");
			ImGui::TableSetupColumn("Id");
			ImGui::TableHeadersRow();

			const auto& assets = Assets.GetAssets();
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(assets.size()), thumbnailSize + ImGui::GetStyle().CellPadding.y * 2.0f);
			while (clipper.Step())
			{
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
				{
					const AssetInfo& asset = assets[row];
					ImGui::PushID(row);
					ImGui::TableNextRow(ImGuiTableRowFlags_None, thumbnailSize);

					// Only rows on screen load a preview.
					ImGui::TableNextColumn();
					if (asset.Type == TextureImporter::TypeName)
					{
						auto [thumbnail, bInserted] = Thumbnails.try_emplace(asset.Id);
						if (bInserted) {
							thumbnail->second = AssetManager::Load<Texture>(asset.Id);
						}
						if (const auto& texture = thumbnail->second) {
							const float scale = thumbnailSize / static_cast<float>(std::max(texture->GetWidth(), texture->GetHeight()));
							ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture->GetSDLTexture())), ImVec2(texture->GetWidth() * scale, texture->GetHeight() * scale));
						}
					}
					else if (asset.Type == TextureImporter::SpriteTypeName)
					{
						// Loading the sprite loads (or reuses) its texture: still one image in memory.
						auto [thumbnail, bInserted] = SpriteThumbnails.try_emplace(asset.Id);
						if (bInserted) {
							thumbnail->second = AssetManager::Load<Sprite>(asset.Id);
						}
						if (const auto& sprite = thumbnail->second) {
							const SpriteRect& rect = sprite->GetRect();
							const Texture& texture = *sprite->GetTexture();
							const float scale = thumbnailSize / static_cast<float>(std::max(rect.Width, rect.Height));
							const ImVec2 uv0(rect.X / static_cast<float>(texture.GetWidth()), rect.Y / static_cast<float>(texture.GetHeight()));
							const ImVec2 uv1((rect.X + rect.Width) / static_cast<float>(texture.GetWidth()), (rect.Y + rect.Height) / static_cast<float>(texture.GetHeight()));
							ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture.GetSDLTexture())), ImVec2(rect.Width * scale, rect.Height * scale), uv0, uv1);
						}
					}

					ImGui::TableNextColumn();
					const std::string label = asset.IsSubAsset() ? "      " + asset.Name : PathToGenericUtf8(asset.Path);
					if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, thumbnailSize))) {
						if (asset.Type == TextureImporter::TypeName) {
							TextureEditorWindow->Open(asset.Id);
						}
						else if (asset.Type == TextureImporter::SpriteTypeName) {
							TextureEditorWindow->Open(asset.ParentId, asset.Id);
						}
					}
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(asset.Type.c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(asset.Id.ToString().c_str());
					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}

		ImGui::End();
	}
}
