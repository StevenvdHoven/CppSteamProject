# CLAUDE.md

## Working rules (from the owner — always follow)
- **Never commit** to any repository. The owner commits themselves.
- **Patch first:** before creating, editing, or deleting any file, show the change as a patch/diff and wait for approval. Only apply after a clear yes.
- **Announce extras:** always say explicitly when something is added that wasn't asked for (extra files, refactors, fixes spotted along the way, new sections).
- **Keep the existing naming conventions**, including the project's own spellings (`Registery`, `Childeren`, `Extention`, `ImGui_OnInpect`, `bIntialised`). Don't "fix" them unless asked.
- Any file may be touched (including `Dependencies/`), but the patch-first rule still applies.
- **Generated files go in `Claude/`** at the repo root (patches, drafts, notes, scratch output, staging copies for test builds). Never write them to the C: drive (no system temp or scratchpad folders).
- Messages from the owner are sometimes sent unfinished. If one ends mid-sentence, ask instead of guessing.
- **FlexEngine is built step by step, system by system.** SpaceAdventure may be used as a reference, but don't copy it blindly: when a better approach exists, present it and discuss it before building it.

## Repository layout
- `FlexEngine/` is the active project: a new 2D engine with a standalone editor that opens projects and exports games. All new work happens here.
- `SpaceAdventure/` is the owner's earlier engine. It's reference only, so don't modify it unless asked.
- `Claude/` holds Claude's patches and working files.

# FlexEngine

## Goals
A lightweight, flexible 2D engine inspired by Godot, Unity and Unreal:
- A standalone editor (`FlexEditor.exe`) that opens **projects**. Each opened project runs in its own editor process.
- Games are written as a C++ **game module** (a DLL per project) that the editor loads.
- The editor exports a final `game.exe` (built from `FlexRuntime`) as **Development** (with debug tools) or **Shipping** (without).

## Build
Visual Studio 2026 (toolset v145, C++20, x64 only). Open `FlexEngine/FlexEngine.sln`. Command line from Git Bash:

```bash
"/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" FlexEngine/FlexEngine.sln -p:Configuration=Debug -p:Platform=x64 -m -v:minimal
```

- To check a change before it's applied, copy `FlexEngine/` to `Claude/work/`, build it there for **all three configurations**, and smoke-test the exes.
- Output goes to `FlexEngine/Binaries/<Config>/` (all exes and DLLs together). Intermediate files go to `FlexEngine/Intermediate/<Project>/<Config>/`. Both are gitignored.
- The debugger's working directory is `Binaries/<Config>/`.
- There are no tests yet.

## Configurations
| | Debug | Development | Shipping |
|---|---|---|---|
| Optimised | no | yes | yes |
| Define | `FLEX_DEBUG=1` | `FLEX_DEVELOPMENT=1` | `FLEX_SHIPPING=1` |
| `FLEX_DEBUG_TOOLS` | 1 | 1 | 0 |
| FlexEditor built | yes | yes | no (`#error` guard) |

- Use `#if FLEX_DEBUG_TOOLS` (not `_DEBUG`) for debug features that should exist in Development games.
- `FLEX_EDITOR=1` is defined **only** in the FlexEditor project. Never use it inside FlexCore, because Core is shared with the runtime.

## Solution structure
- `Build/FlexConfiguration.props` (imported before `Microsoft.Cpp.props`) and `Build/FlexCommon.props` (imported after) hold all shared settings. Every new `.vcxproj` imports both, so settings aren't repeated per project.
- `Source/Core/` → **FlexCore.dll**. Contains all engine systems. Public classes use `FLEX_API` (`Core/Core.h`). It's a DLL so the editor, the runtime and game modules share one copy of the engine state.
- `Source/Editor/` → **FlexEditor.exe**. Editor UI (Dear ImGui docking, compiled into this project only).
- `Source/Runtime/` → **FlexRuntime.exe**. The game player that will become an exported `game.exe`. Console window in Debug/Development, no console in Shipping.
- `Dependencies/` holds vendored libraries: SDL3 3.4.18 (`SDL3/include`, `SDL3/lib/x64`) and Dear ImGui 1.92.9b-docking with its SDL3 + SDL_Renderer3 backends. `SDL3.dll` is copied to `Binaries/` by FlexCore's build.
- Includes are relative to `Source/`: `#include "Core/Engine.h"`.
- **`Core/Graphics/Renderer`** owns the process's single `SDL_Renderer` (`Renderer::Init(window)`). GPU resources are created through it so the backend can change later. **Shutdown order:** release all assets (`AssetManager::Shutdown()` and drop every `shared_ptr`) before `Renderer::Shutdown()`.

## Project system
- A project is a folder holding `<Name>.flexproj` (FlexEngine text format, `[Project version=1]`), `Content/`, `Saved/` (per machine, gitignored) and a `.gitignore`. Project names are letters, digits and `_`, starting with a letter, because they will become game-module names.
- `Flex::Project` (Core) holds **one active project per process**. Opening another project always starts a new `FlexEditor.exe` process (`EditorApp::LaunchEditor`), so nothing ever gets unloaded.
- `FlexEditor.exe` with no arguments shows the `ProjectBrowser`. `FlexEditor.exe --project <file>` (or just `<file>.flexproj`) opens the editor for that project.
- The ImGui layout is saved per project in `Saved/EditorLayout.ini`. Recent projects are kept in `EditorPreferences.ini` next to `FlexEditor.exe`, so nothing is written to AppData.
- Strings are UTF-8 everywhere. Convert with `Utf8ToPath` / `PathToUtf8` (`Core/FileSystem.h`), never by implicit `std::filesystem::path` ↔ `std::string` conversion.
- Native file dialogs (`Editor/FileDialog`) call back on another thread. Results are polled on the main thread.

## Serialization
- **Files in a project** (`.flexproj`, `.meta`, and later scenes/prefabs) use the FlexEngine **text format** (`Core/Serialization/TextDocument`): `[Type attr=value]` sections, then `Key = Value` lines, with `;` comments. Load + save keeps unknown keys, sections and comments. Values are `true`, `-12`, `0.1` (shortest round-trip), `"escaped string"` and `uid(16 hex)`.
- **Exported games** will read **cooked binary** files (`BinaryWriter`/`BinaryReader`). These are order-based with no field names, and rebuilt on every export. Never use binary for files inside a project.
- Classes save and load through **one** `Serialize(Archive&)` function. Call `SerializeVersion(CurrentVersion)` first and branch on the returned version for upgrades. Missing or unreadable fields keep their defaults and add a warning, so loading never hard-fails.
- Paths written into files use `/` separators (`PathToGenericUtf8`).
- Planned: reflection (`FLEX_PROPERTY`) on top of `Archive`, so one declaration gives saving, the inspector and undo.

## Assets
- `Flex::Uid` is a random 64-bit id written as 16 hex characters, and 0 means none. **Assets reference each other by Uid, never by path.**
- `Editor/AssetDatabase` scans `Content/`. Every file of a registered type gets a sidecar `<file>.meta` (`[Meta version=1]`, `Id`, `Type`, plus later import settings) that must be committed and moved together with the file.
- On refresh it creates missing `.meta` files and gives copies (duplicate ids, including sub-asset ids) new ids. The file that owned the id at the previous refresh keeps it: that's remembered in memory and in `Saved/AssetOwners.cache` (per machine). Only without any history does the first path alphabetically win. It **never deletes**: an orphaned `.meta` is only reported, and a `.meta` it can't parse is left untouched and the asset skipped.
- **Sub-assets:** `RegisterSubAssetType(parent, section, type)` turns every `[section]` in a parent's `.meta` into its own asset (`AssetInfo::ParentId`, `Name`). They're listed right after their parent. `FindByPath` only returns file assets.
- **`AssetManager`** (Core) loads assets **by Uid** through a pluggable `AssetSource`, and shares them: the same Uid always gives the same `std::shared_ptr`. It only keeps weak references, so an asset is freed when its last user lets go. The editor plugs in `EditorAssetSource` (source files + `.meta`); exported games will plug in a cooked-data source. Failed loads aren't retried until `AssetManager::ClearFailures()`.
- **Textures:** `.png` and `.bmp`, loaded by SDL 3.4 itself (no SDL_image). Import settings live in the `.meta` `[Import]` section; for now that's `Filter` = `"PixelArt"` (default), `"Nearest"` or `"Linear"`. Editor-side code is in `Editor/TextureImporter`. Changing the filter applies at once to every user of the texture.
- **Sprites** (`Core/Assets/Sprite`) are sub-assets stored as `[Sprite version=1]` sections in their texture's `.meta`: `Id`, `Name`, `Rect.X/Y/Width/Height` (pixels, top-left origin) and `Pivot.X/Y` (0-1 inside the rect, y down). A `Sprite` holds a `shared_ptr` to its `Texture`, so any number of sprites use **one** GPU texture, and the texture is freed when the last sprite and every other user let go. `Sprite::ApplyDefinition` updates an already-loaded sprite after an edit.
- **Texture Editor:** the filter is saved immediately. Sprite edits (drag to cut, select, name/rect/pivot, grid slicing with `Editor/SpriteSlicer`) stay a working copy until **Apply** writes them to the `.meta` and refreshes the database. Re-slicing with "Replace" **keeps the Id of every sprite whose name stays the same**, so references survive. Sprite names should be unique per texture.
- The editor's "Assets" panel (with thumbnails and load statistics) is a temporary debug view until there's a content browser. Thumbnails keep their textures loaded while the editor runs. Clicking a texture opens the **Texture Editor** (preview + import settings).

## Code conventions
- Everything in `namespace Flex`.
- Classes, methods and members in PascalCase; booleans with a `b` prefix (`bIsRunning`).
- Tabs for indentation, `#pragma once`. Function braces go on a new line; control-flow braces go on the same line.
- New `.cpp`/`.h` files must be added to the matching `.vcxproj`.
- Every module must use the same CRT (`/MDd` Debug, `/MD` otherwise), because STL types cross the DLL boundary. The props files already handle this.

# SpaceAdventure (reference only)
A personal SDL2 + ImGui 2D engine/editor in one VS project (`SpaceAdventure/SpaceAdventure.sln`, Debug|x64 = editor, Release = game). It has a GameObject/Component model with macro-based self-registering registries (`DECLARE_COMPONENT`, `ASSET_LOOKUP`, `DECLARE_SYSTEM`), lazily computed `Transform`s, `.asset` binary files under `Content/` loaded by `AssetManager`, and `SceneManager`/`Renderer` singletons. Known weak spots that FlexEngine should avoid:
- Unversioned binary serialization.
- Scene assets being moved out of the asset manager when a scene is opened.
- Renderer/camera registrations that are never removed.
- Build output committed to git.
