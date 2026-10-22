#pragma once
#include "Editor/EditorWindow.h"
#include "Editor/SpriteSlicer.h"
#include "Core/Assets/Sprite.h"
#include "Core/Assets/Texture.h"
#include "Core/Uid.h"
#include <memory>
#include <string>
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace Flex
{
	class AssetDatabase;
	struct EditorContext;

	// Editor window for one texture:
	//  - preview + import settings (filter, saved immediately)
	//  - sprites cut from it: drag on the image to cut one, click to select,
	//    edit name/rect/pivot, or slice the whole sheet on a grid.
	// Sprite edits are a working copy until "Apply" writes them to the .meta.
	// One window per open texture; closing it with unapplied sprite edits asks first.
	class TextureEditor final : public AssetEditor
	{
	public:
		// selectSpriteId: optional sprite to select once the texture is open.
		TextureEditor(EditorContext& context, Uid textureId, Uid selectSpriteId = {});

		void Draw() override;
		void OnReopen(Uid subAssetId) override { SelectSprite(subAssetId); }

		bool HasUnsavedChanges() const { return EditSprites != SavedSprites; }

	private:
		void Load();
		void SelectSprite(Uid spriteId);
		void DrawSettings();
		void DrawCanvas();
		void DrawSidebar();
		void DrawSpriteList();
		void DrawSelectedSprite();
		void DrawGridSlice();
		void DrawUnsavedChangesPopup();

		void DrawSpriteOverlays(ImDrawList* drawList, const ImVec2& imageMin);
		void HandleCanvasInput(ImDrawList* drawList, const ImVec2& imageMin, bool bHovered);

		void ApplySprites();
		void RevertSprites();
		std::string DefaultSpriteNamePrefix() const;

	private:
		EditorContext& Context;
		AssetDatabase& Database;

		Uid TextureId;
		std::shared_ptr<Texture> LoadedTexture; // keeps the texture loaded while it's open
		TextureImportSettings Settings;
		std::string ErrorMessage;

		bool bFitToWindow = true;
		float Zoom = 1.0f;

		// Sprites
		std::vector<SpriteDefinition> EditSprites;  // working copy
		std::vector<SpriteDefinition> SavedSprites; // as on disk
		int SelectedSprite = -1;
		bool bCreatingSprite = false;
		int32_t CreateStartX = 0;
		int32_t CreateStartY = 0;

		// Grid slicing
		GridSliceSettings Slice;
		char SliceNamePrefix[64]{};
		int SlicePivotPreset = 0;
		bool bSliceReplaceExisting = true;
		bool bShowSlicePreview = false;
		std::string SliceMessage;

		// Closing with unapplied sprite edits asks first.
		bool bRequestUnsavedPopup = false;
	};
}
