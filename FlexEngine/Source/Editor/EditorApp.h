#pragma once
#include "Editor/AssetDatabase.h"
#include "Editor/FileDialog.h"
#include "Core/Assets/Sprite.h"
#include "Core/Assets/Texture.h"
#include <unordered_map>
#include <filesystem>
#include <memory>
#include <string>

struct SDL_Window;

namespace Flex
{
	class EditorAssetSource;
	class ProjectBrowser;
	class TextureEditor;

	// One editor process = at most one project.
	//  - Started without a project: shows the ProjectBrowser.
	//  - Started with --project <file>: opens that project in the editor.
	// Opening or creating another project always starts a new FlexEditor process.
	class EditorApp
	{
	public:
		EditorApp();
		~EditorApp();

		bool Init(const std::filesystem::path& projectFile);
		void Run();
		void Shutdown();

		void RequestQuit() { bIsRunning = false; }
		SDL_Window* GetWindow() const { return Window; }

		// Starts a new FlexEditor process. An empty path opens the project browser.
		static bool LaunchEditor(const std::filesystem::path& projectFile);

	private:
		bool CreateWindowAndImGui(const std::string& title, int width, int height, const std::filesystem::path& imguiIniFile);
		void DrawProjectEditor();
		void DrawMainMenuBar();
		void DrawAssetsPanel();
		void RefreshAssets();

	private:
		SDL_Window* Window = nullptr;
		bool bIsRunning = false;

		// Only exists while no project is open.
		std::unique_ptr<ProjectBrowser> Browser;

		FileDialog OpenProjectDialog;

		AssetDatabase Assets;
		AssetRefreshReport LastAssetRefresh;
		std::unique_ptr<EditorAssetSource> AssetSourceForEditor;
		std::unique_ptr<TextureEditor> TextureEditorWindow;

		// Small previews in the Assets panel. Holding them keeps those textures loaded
		// while the editor runs; cleared on refresh.
		std::unordered_map<Uid, std::shared_ptr<Texture>> Thumbnails;
		std::unordered_map<Uid, std::shared_ptr<Sprite>> SpriteThumbnails; // share their texture with Thumbnails

		// ImGui keeps a pointer to this string, so it has to outlive the ImGui context.
		std::string ImGuiIniFile;
	};
}
