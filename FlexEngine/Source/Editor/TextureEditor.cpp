#include "Editor/TextureEditor.h"
#include "Editor/AssetDatabase.h"
#include "Editor/EditorContext.h"
#include "Editor/TextureImporter.h"
#include "Core/Assets/AssetManager.h"
#include "Core/FileSystem.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Flex
{
	namespace
	{
		struct PivotPreset
		{
			const char* Name;
			float X;
			float Y;
		};

		constexpr PivotPreset PIVOTPRESETS[] = {
			{ "Center", 0.5f, 0.5f },
			{ "Bottom Center", 0.5f, 1.0f },
			{ "Top Left", 0.0f, 0.0f },
			{ "Top Center", 0.5f, 0.0f },
			{ "Bottom Left", 0.0f, 1.0f },
			{ "Bottom Right", 1.0f, 1.0f },
		};
		constexpr int PIVOTPRESETCOUNT = static_cast<int>(sizeof(PIVOTPRESETS) / sizeof(PIVOTPRESETS[0]));

		constexpr const char* UNSAVEDPOPUP = "Unsaved sprite changes";

		ImTextureID ToImTexture(const std::shared_ptr<Texture>& texture)
		{
			return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture->GetSDLTexture()));
		}

		// Grey checkerboard behind the image so transparent pixels are visible.
		void DrawCheckerboard(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float cellSize)
		{
			const ImU32 light = IM_COL32(110, 110, 110, 255);
			const ImU32 dark = IM_COL32(80, 80, 80, 255);
			drawList->AddRectFilled(min, max, dark);

			int row = 0;
			for (float y = min.y; y < max.y; y += cellSize, ++row)
			{
				int column = 0;
				for (float x = min.x; x < max.x; x += cellSize, ++column)
				{
					if ((row + column) % 2 == 0) {
						drawList->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + cellSize, max.x), std::min(y + cellSize, max.y)), light);
					}
				}
			}
		}

		int FindPivotPreset(float x, float y)
		{
			for (int index{ 0 }; index < PIVOTPRESETCOUNT; ++index) {
				if (PIVOTPRESETS[index].X == x && PIVOTPRESETS[index].Y == y) {
					return index;
				}
			}
			return -1;
		}
	}

	// -----------------------
	// Opening / closing
	// -----------------------

	TextureEditor::TextureEditor(EditorContext& context, Uid textureId, Uid selectSpriteId)
		: AssetEditor("Texture Editor", "TextureEditor", textureId)
		, Context{ context }
		, Database{ context.Assets }
		, TextureId{ textureId }
	{
		if (const AssetInfo* asset = Database.Find(textureId)) {
			SetTitle(asset->Name + " - Texture Editor");
		}
		Load();
		SelectSprite(selectSpriteId);
	}

	void TextureEditor::Load()
	{
		const AssetInfo* asset = Database.Find(TextureId);
		if (!asset || asset->Type != TextureImporter::TypeName) {
			ErrorMessage = "This texture no longer exists.";
			return;
		}

		std::snprintf(SliceNamePrefix, sizeof(SliceNamePrefix), "%s", DefaultSpriteNamePrefix().c_str());
		if (TextureImporter::ReadSettings(Database, *asset, Settings, ErrorMessage) &&
			TextureImporter::ReadSprites(Database, *asset, SavedSprites, ErrorMessage))
		{
			EditSprites = SavedSprites;
			LoadedTexture = AssetManager::Load<Texture>(TextureId);
			if (!LoadedTexture) {
				ErrorMessage = "Could not load the texture (see the log for details).";
			}
		}
	}

	void TextureEditor::SelectSprite(Uid spriteId)
	{
		if (!spriteId.IsValid()) {
			return;
		}
		for (int index{ 0 }; index < static_cast<int>(EditSprites.size()); ++index) {
			if (EditSprites[index].Id == spriteId) {
				SelectedSprite = index;
			}
		}
	}

	std::string TextureEditor::DefaultSpriteNamePrefix() const
	{
		const AssetInfo* asset = Database.Find(TextureId);
		return asset ? asset->Name + "_" : std::string("Sprite_");
	}

	// -----------------------
	// Window
	// -----------------------

	void TextureEditor::Draw()
	{
		ImGui::SetNextWindowSize(ImVec2(1000.0f, 650.0f), ImGuiCond_FirstUseEver);
		const bool bVisible = BeginWindow();

		// The tab's close button: keep the window open and ask when sprite edits aren't applied.
		if (!bIsOpen && HasUnsavedChanges()) {
			bIsOpen = true;
			bRequestUnsavedPopup = true;
		}
		if (!bVisible) {
			EndWindow();
			return;
		}

		if (bRequestUnsavedPopup) {
			ImGui::OpenPopup(UNSAVEDPOPUP);
			bRequestUnsavedPopup = false;
		}
		DrawUnsavedChangesPopup();

		const AssetInfo* asset = TextureId.IsValid() ? Database.Find(TextureId) : nullptr;
		if (!asset) {
			ImGui::TextDisabled("This texture no longer exists.");
			EndWindow();
			return;
		}

		ImGui::Text("%s", PathToGenericUtf8(asset->Path).c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("%s", TextureId.ToString().c_str());
		if (HasUnsavedChanges()) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "(unsaved sprite changes)");
		}

		if (!ErrorMessage.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", ErrorMessage.c_str());
		}

		if (LoadedTexture)
		{
			DrawSettings();
			ImGui::Separator();

			if (ImGui::BeginTable("TextureEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 330.0f);
				ImGui::TableNextColumn();
				DrawCanvas();
				ImGui::TableNextColumn();
				DrawSidebar();
				ImGui::EndTable();
			}
		}

		EndWindow();
	}

	void TextureEditor::DrawSettings()
	{
		ImGui::Text("Size: %d x %d   GPU memory: %.1f KB   Sprites: %d (sharing this one texture)",
			LoadedTexture->GetWidth(), LoadedTexture->GetHeight(), LoadedTexture->GetMemorySize() / 1024.0f, static_cast<int>(SavedSprites.size()));

		ImGui::SetNextItemWidth(160.0f);
		if (ImGui::BeginCombo("Filter", ToString(Settings.Filter)))
		{
			for (TextureFilter filter : { TextureFilter::PixelArt, TextureFilter::Nearest, TextureFilter::Linear })
			{
				if (ImGui::Selectable(ToString(filter), Settings.Filter == filter) && Settings.Filter != filter)
				{
					Settings.Filter = filter;
					LoadedTexture->SetFilter(filter); // every user of this texture sees it right away

					const AssetInfo* asset = Database.Find(TextureId);
					if (asset && !TextureImporter::WriteSettings(Database, *asset, Settings, ErrorMessage)) {
						ErrorMessage = "Could not save the .meta: " + ErrorMessage;
					}
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::BeginItemTooltip()) {
			ImGui::TextUnformatted("PixelArt: sharp pixels, smooth at any zoom.\nNearest: classic sharp pixels.\nLinear: smooth, for high-resolution art.\n\nSaved immediately.");
			ImGui::EndTooltip();
		}

		ImGui::SameLine();
		ImGui::Checkbox("Fit", &bFitToWindow);
		if (!bFitToWindow) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(200.0f);
			ImGui::SliderFloat("Zoom", &Zoom, 0.05f, 32.0f, "%.2fx", ImGuiSliderFlags_Logarithmic);
		}
	}

	// -----------------------
	// Canvas
	// -----------------------

	void TextureEditor::DrawCanvas()
	{
		ImGui::BeginChild("Canvas", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

		const float width = static_cast<float>(LoadedTexture->GetWidth());
		const float height = static_cast<float>(LoadedTexture->GetHeight());
		if (bFitToWindow) {
			const ImVec2 available = ImGui::GetContentRegionAvail();
			Zoom = std::clamp(std::min(available.x / width, available.y / height), 0.05f, 32.0f);
		}

		const ImVec2 size(std::max(width * Zoom, 1.0f), std::max(height * Zoom, 1.0f));
		const ImVec2 min = ImGui::GetCursorScreenPos();
		const ImVec2 max(min.x + size.x, min.y + size.y);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		DrawCheckerboard(drawList, min, max, 8.0f);
		drawList->AddImage(ToImTexture(LoadedTexture), min, max);

		ImGui::InvisibleButton("CanvasInput", size);
		const bool bHovered = ImGui::IsItemHovered();

		HandleCanvasInput(drawList, min, bHovered);
		DrawSpriteOverlays(drawList, min);

		// Ctrl + mouse wheel zooms (plain wheel keeps scrolling the canvas).
		const ImGuiIO& io = ImGui::GetIO();
		if (bHovered && io.KeyCtrl && io.MouseWheel != 0.0f) {
			bFitToWindow = false;
			Zoom = std::clamp(Zoom * (io.MouseWheel > 0.0f ? 1.25f : 0.8f), 0.05f, 32.0f);
		}

		ImGui::EndChild();
	}

	void TextureEditor::HandleCanvasInput(ImDrawList* drawList, const ImVec2& imageMin, bool bHovered)
	{
		const int32_t textureWidth = LoadedTexture->GetWidth();
		const int32_t textureHeight = LoadedTexture->GetHeight();
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		const bool bAltHeld = ImGui::GetIO().KeyAlt;

		// Nearest pixel EDGE under the mouse (0..size), used for cutting.
		const int32_t edgeX = std::clamp(static_cast<int32_t>(std::lround((mouse.x - imageMin.x) / Zoom)), 0, textureWidth);
		const int32_t edgeY = std::clamp(static_cast<int32_t>(std::lround((mouse.y - imageMin.y) / Zoom)), 0, textureHeight);

		if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			// The pixel under the mouse, used for picking (last drawn = on top).
			const int32_t pixelX = static_cast<int32_t>(std::floor((mouse.x - imageMin.x) / Zoom));
			const int32_t pixelY = static_cast<int32_t>(std::floor((mouse.y - imageMin.y) / Zoom));

			int hit = -1;
			for (int index = static_cast<int>(EditSprites.size()) - 1; index >= 0; --index)
			{
				const SpriteRect& rect = EditSprites[index].Rect;
				if (pixelX >= rect.X && pixelX < rect.X + rect.Width && pixelY >= rect.Y && pixelY < rect.Y + rect.Height) {
					hit = index;
					break;
				}
			}

			if (hit >= 0 && !bAltHeld) {
				SelectedSprite = hit;
			}
			else {
				bCreatingSprite = true;
				CreateStartX = edgeX;
				CreateStartY = edgeY;
			}
		}

		if (!bCreatingSprite) {
			return;
		}

		SpriteRect rect;
		rect.X = std::min(CreateStartX, edgeX);
		rect.Y = std::min(CreateStartY, edgeY);
		rect.Width = std::abs(edgeX - CreateStartX);
		rect.Height = std::abs(edgeY - CreateStartY);

		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			const ImVec2 a(imageMin.x + rect.X * Zoom, imageMin.y + rect.Y * Zoom);
			const ImVec2 b(a.x + rect.Width * Zoom, a.y + rect.Height * Zoom);
			drawList->AddRectFilled(a, b, IM_COL32(255, 255, 255, 40));
			drawList->AddRect(a, b, IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.5f);
			ImGui::SetTooltip("%d x %d", rect.Width, rect.Height);
			return;
		}

		// Released: a real drag cuts a sprite, a plain click on empty space deselects.
		bCreatingSprite = false;
		if (rect.Width >= 2 && rect.Height >= 2)
		{
			SpriteDefinition sprite;
			sprite.Id = Uid::Generate();
			sprite.Name = SpriteSlicer::MakeUniqueName(DefaultSpriteNamePrefix() + std::to_string(EditSprites.size()), EditSprites);
			sprite.Rect = rect.ClampedTo(textureWidth, textureHeight);
			EditSprites.push_back(std::move(sprite));
			SelectedSprite = static_cast<int>(EditSprites.size()) - 1;
		}
		else {
			SelectedSprite = -1;
		}
	}

	void TextureEditor::DrawSpriteOverlays(ImDrawList* drawList, const ImVec2& imageMin)
	{
		auto toScreen = [&](const SpriteRect& rect, ImVec2& outMin, ImVec2& outMax) {
			outMin = ImVec2(imageMin.x + rect.X * Zoom, imageMin.y + rect.Y * Zoom);
			outMax = ImVec2(outMin.x + rect.Width * Zoom, outMin.y + rect.Height * Zoom);
		};

		if (bShowSlicePreview)
		{
			for (const SpriteRect& rect : SpriteSlicer::SliceGrid(Slice, LoadedTexture->GetWidth(), LoadedTexture->GetHeight()))
			{
				ImVec2 a;
				ImVec2 b;
				toScreen(rect, a, b);
				drawList->AddRect(a, b, IM_COL32(255, 150, 0, 160));
			}
		}

		for (int index{ 0 }; index < static_cast<int>(EditSprites.size()); ++index)
		{
			const SpriteDefinition& sprite = EditSprites[index];
			const bool bSelected = index == SelectedSprite;
			const ImU32 color = bSelected ? IM_COL32(255, 220, 0, 255) : IM_COL32(0, 200, 255, 200);

			ImVec2 a;
			ImVec2 b;
			toScreen(sprite.Rect, a, b);
			drawList->AddRect(a, b, color, 0.0f, 0, bSelected ? 2.0f : 1.0f);

			if (b.x - a.x > 50.0f) {
				drawList->AddText(ImVec2(a.x + 3.0f, a.y + 2.0f), color, sprite.Name.c_str());
			}
			if (bSelected) {
				const ImVec2 pivot(a.x + (b.x - a.x) * sprite.PivotX, a.y + (b.y - a.y) * sprite.PivotY);
				drawList->AddCircleFilled(pivot, 4.0f, IM_COL32(255, 60, 200, 255));
			}
		}
	}

	// -----------------------
	// Sidebar
	// -----------------------

	void TextureEditor::DrawSidebar()
	{
		ImGui::BeginChild("Sidebar");

		DrawSpriteList();
		DrawSelectedSprite();
		DrawGridSlice();

		ImGui::SeparatorText("Changes");
		const bool bDirty = HasUnsavedChanges();
		ImGui::BeginDisabled(!bDirty);
		if (ImGui::Button("Apply")) {
			ApplySprites();
		}
		ImGui::SameLine();
		if (ImGui::Button("Revert")) {
			RevertSprites();
		}
		ImGui::EndDisabled();
		if (bDirty) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Not saved yet");
		}

		ImGui::EndChild();
	}

	void TextureEditor::DrawSpriteList()
	{
		ImGui::SeparatorText(("Sprites (" + std::to_string(EditSprites.size()) + ")").c_str());

		if (ImGui::BeginListBox("##Sprites", ImVec2(-FLT_MIN, 8.0f * ImGui::GetTextLineHeightWithSpacing())))
		{
			for (int index{ 0 }; index < static_cast<int>(EditSprites.size()); ++index)
			{
				const SpriteDefinition& sprite = EditSprites[index];
				const std::string label = sprite.Name + "  (" + std::to_string(sprite.Rect.Width) + " x " + std::to_string(sprite.Rect.Height) + ")";
				ImGui::PushID(index);
				if (ImGui::Selectable(label.c_str(), index == SelectedSprite)) {
					SelectedSprite = index;
				}
				ImGui::PopID();
			}
			ImGui::EndListBox();
		}

		ImGui::BeginDisabled(SelectedSprite < 0);
		if (ImGui::Button("Delete")) {
			EditSprites.erase(EditSprites.begin() + SelectedSprite);
			SelectedSprite = std::min(SelectedSprite, static_cast<int>(EditSprites.size()) - 1);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(EditSprites.empty());
		if (ImGui::Button("Delete All")) {
			EditSprites.clear();
			SelectedSprite = -1;
		}
		ImGui::EndDisabled();

		ImGui::TextDisabled("Drag on the image to cut a sprite.\nClick a sprite to select it (Alt+drag cuts on top).\nCtrl+wheel zooms.");
	}

	void TextureEditor::DrawSelectedSprite()
	{
		if (SelectedSprite < 0 || SelectedSprite >= static_cast<int>(EditSprites.size())) {
			return;
		}

		SpriteDefinition& sprite = EditSprites[SelectedSprite];
		ImGui::SeparatorText("Sprite");

		char nameBuffer[128];
		std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", sprite.Name.c_str());
		if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
			sprite.Name = nameBuffer;
		}
		const auto sameName = std::count_if(EditSprites.begin(), EditSprites.end(), [&](const SpriteDefinition& other) { return other.Name == sprite.Name; });
		if (sameName > 1 || sprite.Name.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), sprite.Name.empty() ? "Name is empty." : "Another sprite has this name (re-slicing matches by name).");
		}

		int position[2] = { sprite.Rect.X, sprite.Rect.Y };
		int size[2] = { sprite.Rect.Width, sprite.Rect.Height };
		bool bRectChanged = ImGui::InputInt2("Position", position);
		bRectChanged |= ImGui::InputInt2("Size", size);
		if (bRectChanged) {
			sprite.Rect = SpriteRect{ position[0], position[1], size[0], size[1] }.ClampedTo(LoadedTexture->GetWidth(), LoadedTexture->GetHeight());
		}

		const int preset = FindPivotPreset(sprite.PivotX, sprite.PivotY);
		if (ImGui::BeginCombo("Pivot", preset >= 0 ? PIVOTPRESETS[preset].Name : "Custom"))
		{
			for (int index{ 0 }; index < PIVOTPRESETCOUNT; ++index) {
				if (ImGui::Selectable(PIVOTPRESETS[index].Name, index == preset)) {
					sprite.PivotX = PIVOTPRESETS[index].X;
					sprite.PivotY = PIVOTPRESETS[index].Y;
				}
			}
			ImGui::EndCombo();
		}
		float pivot[2] = { sprite.PivotX, sprite.PivotY };
		if (ImGui::DragFloat2("Pivot (0-1)", pivot, 0.01f, 0.0f, 1.0f, "%.2f")) {
			sprite.PivotX = std::clamp(pivot[0], 0.0f, 1.0f);
			sprite.PivotY = std::clamp(pivot[1], 0.0f, 1.0f);
		}

		ImGui::TextDisabled("Id: %s", sprite.Id.ToString().c_str());
	}

	void TextureEditor::DrawGridSlice()
	{
		bShowSlicePreview = ImGui::CollapsingHeader("Grid Slice");
		if (!bShowSlicePreview) {
			return;
		}

		int mode = static_cast<int>(Slice.Mode);
		ImGui::RadioButton("Cell size", &mode, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Cell count", &mode, 1);
		Slice.Mode = static_cast<GridSliceSettings::SliceMode>(mode);

		if (Slice.Mode == GridSliceSettings::SliceMode::CellSize) {
			int cell[2] = { Slice.CellWidth, Slice.CellHeight };
			if (ImGui::InputInt2("Cell size", cell)) {
				Slice.CellWidth = std::max(cell[0], 1);
				Slice.CellHeight = std::max(cell[1], 1);
			}
		}
		else {
			int count[2] = { Slice.Columns, Slice.Rows };
			if (ImGui::InputInt2("Columns, rows", count)) {
				Slice.Columns = std::max(count[0], 1);
				Slice.Rows = std::max(count[1], 1);
			}
		}

		int offset[2] = { Slice.OffsetX, Slice.OffsetY };
		if (ImGui::InputInt2("Offset", offset)) {
			Slice.OffsetX = std::max(offset[0], 0);
			Slice.OffsetY = std::max(offset[1], 0);
		}
		int padding[2] = { Slice.PaddingX, Slice.PaddingY };
		if (ImGui::InputInt2("Padding", padding)) {
			Slice.PaddingX = std::max(padding[0], 0);
			Slice.PaddingY = std::max(padding[1], 0);
		}
		ImGui::Checkbox("Skip empty cells", &Slice.bSkipEmptyCells);
		ImGui::InputText("Name prefix", SliceNamePrefix, sizeof(SliceNamePrefix));

		if (ImGui::BeginCombo("Pivot##Slice", PIVOTPRESETS[SlicePivotPreset].Name))
		{
			for (int index{ 0 }; index < PIVOTPRESETCOUNT; ++index) {
				if (ImGui::Selectable(PIVOTPRESETS[index].Name, index == SlicePivotPreset)) {
					SlicePivotPreset = index;
				}
			}
			ImGui::EndCombo();
		}

		int replace = bSliceReplaceExisting ? 0 : 1;
		ImGui::RadioButton("Replace sprites", &replace, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Add to sprites", &replace, 1);
		bSliceReplaceExisting = replace == 0;

		const std::vector<SpriteRect> preview = SpriteSlicer::SliceGrid(Slice, LoadedTexture->GetWidth(), LoadedTexture->GetHeight());
		ImGui::TextDisabled("%d cells (orange)%s", static_cast<int>(preview.size()), preview.size() >= SpriteSlicer::MaxSprites ? " - limit reached" : "");

		ImGui::BeginDisabled(preview.empty());
		if (ImGui::Button("Slice"))
		{
			std::vector<SpriteRect> rects = preview;
			const size_t cellCount = rects.size();
			std::string error;
			const AssetInfo* asset = Database.Find(TextureId);
			if (Slice.bSkipEmptyCells && asset && !SpriteSlicer::RemoveEmptyCells(Database.GetSourcePath(*asset), rects, error)) {
				SliceMessage = error;
			}
			else
			{
				const PivotPreset& pivot = PIVOTPRESETS[SlicePivotPreset];
				if (bSliceReplaceExisting)
				{
					// Same name as before = same Id, so references survive a re-slice.
					EditSprites = SpriteSlicer::BuildSprites(rects, SliceNamePrefix, EditSprites, pivot.X, pivot.Y);
				}
				else
				{
					for (SpriteDefinition& sprite : SpriteSlicer::BuildSprites(rects, SliceNamePrefix, {}, pivot.X, pivot.Y)) {
						sprite.Name = SpriteSlicer::MakeUniqueName(sprite.Name, EditSprites);
						EditSprites.push_back(std::move(sprite));
					}
				}
				SelectedSprite = -1;
				SliceMessage = std::to_string(rects.size()) + " sprite(s) created";
				if (cellCount != rects.size()) {
					SliceMessage += ", " + std::to_string(cellCount - rects.size()) + " empty cell(s) skipped";
				}
				SliceMessage += ". Click Apply to save.";
			}
		}
		ImGui::EndDisabled();

		if (!SliceMessage.empty()) {
			ImGui::TextWrapped("%s", SliceMessage.c_str());
		}
	}

	void TextureEditor::DrawUnsavedChangesPopup()
	{
		if (!ImGui::BeginPopupModal(UNSAVEDPOPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			return;
		}

		ImGui::TextUnformatted("The sprites of this texture have changes that aren't applied yet.");
		if (ImGui::Button("Apply and Close"))
		{
			ApplySprites();
			if (!HasUnsavedChanges()) {
				bIsOpen = false;
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard and Close"))
		{
			RevertSprites();
			bIsOpen = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// -----------------------
	// Saving
	// -----------------------

	void TextureEditor::ApplySprites()
	{
		const AssetInfo* asset = Database.Find(TextureId);
		if (!asset || !LoadedTexture) {
			return;
		}

		for (SpriteDefinition& sprite : EditSprites) {
			sprite.Rect = sprite.Rect.ClampedTo(LoadedTexture->GetWidth(), LoadedTexture->GetHeight());
		}

		std::string error;
		if (!TextureImporter::WriteSprites(Database, *asset, EditSprites, error)) {
			ErrorMessage = "Could not save sprites: " + error;
			return;
		}
		SavedSprites = EditSprites;
		ErrorMessage.clear();

		// Sprites that are already loaded somewhere pick up their new rect/pivot right away.
		for (const SpriteDefinition& definition : EditSprites) {
			if (auto loaded = std::dynamic_pointer_cast<Sprite>(AssetManager::FindLoaded(definition.Id))) {
				loaded->ApplyDefinition(definition);
			}
		}

		// Picks up new/removed sprite ids. Note: refreshes the database, so `asset` is invalid after this.
		if (Context.RefreshAssets) {
			Context.RefreshAssets();
		}
	}

	void TextureEditor::RevertSprites()
	{
		EditSprites = SavedSprites;
		SelectedSprite = -1;
		bCreatingSprite = false;
	}
}
