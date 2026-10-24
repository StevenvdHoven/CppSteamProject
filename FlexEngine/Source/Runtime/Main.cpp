#include "Core/Engine.h"
#include "Core/Assets/AssetManager.h"
#include "Core/Assets/ContentAssetSource.h"
#include "Core/FileSystem.h"
#include "Core/Game/GameInstance.h"
#include "Core/Game/GameModule.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Graphics/SpriteBatch.h"
#include "Core/Input/Input.h"
#include "Core/Project.h"
#include "Core/Scene/Scene.h"
#include "Core/Scene/SceneSerializer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

// The runtime is the player that will become an exported game.exe.
//
//     FlexRuntime.exe --project <file.flexproj> [--scene <file.scene>] [--benchmark SECONDS]
//
// With a project it loads the project's game module, opens the scene (the project's start
// scene unless --scene is given) and runs it. Without --project it runs the .flexproj next to
// the exe, if there is exactly one: that's how a game made with Build Project starts. Assets come straight from Content/ for now;
// exported games will read cooked data. --benchmark prints average timings after SECONDS
// (after a one second warm-up) and exits. Without any project it only opens a window.

namespace
{
	struct RuntimeOptions
	{
		std::filesystem::path ProjectFile;
		std::filesystem::path SceneFile;
		double BenchmarkSeconds = 0.0;
	};

	RuntimeOptions ParseOptions(int argc, char** argv)
	{
		RuntimeOptions options;
		for (int i = 1; i + 1 < argc; ++i) {
			if (std::strcmp(argv[i], "--project") == 0) {
				options.ProjectFile = Flex::Utf8ToPath(argv[++i]);
			} else if (std::strcmp(argv[i], "--scene") == 0) {
				options.SceneFile = Flex::Utf8ToPath(argv[++i]);
			} else if (std::strcmp(argv[i], "--benchmark") == 0) {
				options.BenchmarkSeconds = std::strtod(argv[++i], nullptr);
			}
		}
		if (options.ProjectFile.empty()) {
			if (const char* basePath = SDL_GetBasePath()) {
				std::error_code error;
				std::filesystem::path found;
				int count = 0;
				for (const auto& entry : std::filesystem::directory_iterator(Flex::Utf8ToPath(basePath), error)) {
					if (entry.path().extension() == ".flexproj") {
						found = entry.path();
						++count;
					}
				}
				if (count == 1) {
					options.ProjectFile = found;
				}
			}
		}
		return options;
	}

	double ToMilliseconds(uint64_t ticks)
	{
		return static_cast<double>(ticks) * 1000.0 / static_cast<double>(SDL_GetPerformanceFrequency());
	}
}

int main(int argc, char** argv)
{
	using namespace Flex;
	const RuntimeOptions options = ParseOptions(argc, argv);

	if (!Engine::Init()) {
		return 1;
	}

	std::string error;
	Project* project = nullptr;
	if (!options.ProjectFile.empty()) {
		project = Project::Open(options.ProjectFile, error);
		if (!project) {
			SDL_Log("Could not open project: %s", error.c_str());
			Engine::Shutdown();
			return 1;
		}
	}

	const std::string title = project ? project->GetName() + " (" FLEX_BUILD_CONFIG_NAME ")" : std::string("FlexRuntime (" FLEX_BUILD_CONFIG_NAME ")");
	SDL_Window* window = SDL_CreateWindow(title.c_str(), 1280, 720, SDL_WINDOW_RESIZABLE);
	if (!window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		Engine::Shutdown();
		return 1;
	}

	if (!Renderer::Init(window)) {
		SDL_DestroyWindow(window);
		Engine::Shutdown();
		return 1;
	}
	SDL_Renderer* renderer = Renderer::GetSDLRenderer();
	const bool bBenchmark = options.BenchmarkSeconds > 0.0;
	if (bBenchmark) {
		SDL_SetRenderVSync(renderer, 0);
	}

	int exitCode = 0;
	{
		std::unique_ptr<ContentAssetSource> assetSource;
		Scene scene;
		GameInstance game;
		SpriteBatch batch;
		bool bHasGame = false;

		if (project) {
			assetSource = std::make_unique<ContentAssetSource>(project->GetContentDirectory());
			AssetManager::SetSource(assetSource.get());

			const std::filesystem::path moduleFile = GameModule::FindModuleFile(*project);
			if (moduleFile.empty()) {
				SDL_Log("No game module found for %s (expected %s.dll).", project->GetName().c_str(), project->GetName().c_str());
			} else if (!GameModule::Load(moduleFile, error)) {
				SDL_Log("%s", error.c_str());
			}

			std::filesystem::path sceneFile = options.SceneFile;
			if (sceneFile.empty() && project->GetStartScene().IsValid()) {
				sceneFile = assetSource->GetSourcePath(project->GetStartScene());
			}
			std::vector<std::string> warnings;
			if (sceneFile.empty()) {
				SDL_Log("No scene to run: set the project's Start Scene in the editor, or pass --scene.");
			} else if (!SceneSerializer::LoadFromFile(scene, sceneFile, warnings, error)) {
				SDL_Log("Could not load %s: %s", PathToUtf8(sceneFile).c_str(), error.c_str());
			} else {
				for (const std::string& warning : warnings) {
					SDL_Log("%s: %s", PathToUtf8(sceneFile.filename()).c_str(), warning.c_str());
				}
				game.Begin(scene);
				bHasGame = true;
			}
		}

		uint64_t previous = SDL_GetPerformanceCounter();
		double time = 0.0;
		double titleTimer = 0.0;
		uint32_t titleFrames = 0;
		double benchFrame = 0.0, benchTick = 0.0, benchRender = 0.0;
		uint32_t benchFrames = 0;

		bool bIsRunning = true;
		while (bIsRunning)
		{
			Input::BeginFrame();
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				Input::ProcessEvent(event);
				if (event.type == SDL_EVENT_QUIT) {
					bIsRunning = false;
				}
				if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
					Input::ReleaseKeyboardMouse();
				}
			}

			const uint64_t frameStart = SDL_GetPerformanceCounter();
			const double frameSeconds = ToMilliseconds(frameStart - previous) / 1000.0;
			previous = frameStart;
			time += frameSeconds;

			int width = 0;
			int height = 0;
			SDL_GetCurrentRenderOutputSize(renderer, &width, &height);
			int windowWidth = 0;
			int windowHeight = 0;
			SDL_GetWindowSize(window, &windowWidth, &windowHeight);
			Input::SetViewport(Rect(0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight)),
				Vector2(static_cast<float>(width), static_cast<float>(height)));
			if (bHasGame) {
				game.Tick(scene, static_cast<float>(frameSeconds));
				game.Render(scene, batch, { static_cast<float>(width), static_cast<float>(height) });
			} else {
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
				SDL_RenderClear(renderer);
			}
			SDL_RenderPresent(renderer);
			Renderer::EndFrame();
			const double frameMilliseconds = ToMilliseconds(SDL_GetPerformanceCounter() - frameStart);

			const GameInstance::Statistics& stats = game.GetStatistics();
			if (bBenchmark && time > 1.0) {
				benchFrame += frameMilliseconds;
				benchTick += stats.TickMilliseconds;
				benchRender += stats.RenderMilliseconds;
				++benchFrames;
			}

			++titleFrames;
			titleTimer += frameSeconds;
			if (titleTimer >= 0.5 && bHasGame) {
				char text[256];
				std::snprintf(text, sizeof(text), "%s (%s) - %.0f fps | tick %.2f ms | render %.2f ms | %u sprites | %u bodies",
					project->GetName().c_str(), FLEX_BUILD_CONFIG_NAME, titleFrames / titleTimer, stats.TickMilliseconds,
					stats.RenderMilliseconds, stats.SpritesDrawn, stats.Physics.DynamicBodies);
				SDL_SetWindowTitle(window, text);
				titleFrames = 0;
				titleTimer = 0.0;
			}

			if (bBenchmark && time >= options.BenchmarkSeconds + 1.0) {
				bIsRunning = false;
			}
		}

		if (bBenchmark) {
			if (benchFrames == 0) {
				exitCode = 1;
			} else {
				const GameInstance::Statistics& stats = game.GetStatistics();
				std::printf("FlexRuntime benchmark (%s): %u sprites, %u dynamic bodies, %u frames, vsync off\n",
					FLEX_BUILD_CONFIG_NAME, stats.SpritesDrawn, stats.Physics.DynamicBodies, benchFrames);
				std::printf("  game tick (systems + physics + transforms): %.3f ms\n", benchTick / benchFrames);
				std::printf("  render (collect + draw): %.3f ms\n", benchRender / benchFrames);
				std::printf("  whole frame incl. present: %.3f ms (%.0f fps)\n", benchFrame / benchFrames, 1000.0 * benchFrames / benchFrame);
				std::fflush(stdout);
			}
		}

		scene.Clear(); // release the scene's assets before the renderer goes away
		AssetManager::Shutdown();
	}

	Renderer::Shutdown();
	SDL_DestroyWindow(window);
	Project::CloseActive();
	Engine::Shutdown();
	return exitCode;
}
