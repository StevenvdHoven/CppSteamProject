#include "Plugins/Paint/PaintEditor.h"
#include "Editor/AssetDatabase.h"
#include "Editor/EditorContext.h"
#include "Editor/TextureImporter.h"
#include "Core/Assets/AssetManager.h"
#include "Core/Assets/Texture.h"
#include "Core/FileSystem.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_set>

namespace Flex::Paint
{
	namespace
	{
		constexpr const char* UNSAVEDPOPUP = "Unsaved painting";
		constexpr const char* OVERWRITEPOPUP = "Overwrite texture?";
		constexpr const char* SIZEPOPUP = "Painting size";
		constexpr const char* TEXTUREPOPUP = "Import texture as layer";
		constexpr size_t MAXHISTORY = 100;
		constexpr size_t MAXHISTORYBYTES = 512ull * 1024 * 1024;
		constexpr float MINZOOM = 0.25f;
		constexpr float MAXZOOM = 64.0f;

		// Shared by every Paint window, so pixels can be copied from one painting to another.
		Image Clipboard;

		struct ToolInfo
		{
			const char* Name;
			const char* Shortcut;
			ImGuiKey Key;
		};
		constexpr ToolInfo TOOLS[] = {
			{ "Pencil", "N", ImGuiKey_N },
			{ "Brush", "B", ImGuiKey_B },
			{ "Eraser", "E", ImGuiKey_E },
			{ "Fill", "G", ImGuiKey_G },
			{ "Picker", "I", ImGuiKey_I },
			{ "Line", "L", ImGuiKey_L },
			{ "Rectangle", "U", ImGuiKey_U },
			{ "Ellipse", "O", ImGuiKey_O },
			{ "Select", "M", ImGuiKey_M },
			{ "Move", "V", ImGuiKey_V },
		};

		ImTextureID ToImTexture(const std::shared_ptr<Texture>& texture)
		{
			return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture->GetSDLTexture()));
		}

		// A wrapped grey hint line.
		void Hint(const char* text)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::TextWrapped("%s", text);
			ImGui::PopStyleColor();
		}

		void ToFloat4(Pixel pixel, float* out)
		{
			out[0] = pixel.R / 255.0f;
			out[1] = pixel.G / 255.0f;
			out[2] = pixel.B / 255.0f;
			out[3] = pixel.A / 255.0f;
		}

		Pixel FromFloat4(const float* color)
		{
			const auto channel = [](float value) { return static_cast<uint8_t>(std::clamp(value * 255.0f + 0.5f, 0.0f, 255.0f)); };
			return { channel(color[0]), channel(color[1]), channel(color[2]), channel(color[3]) };
		}

		ImVec4 ToImVec4(Pixel pixel)
		{
			return { pixel.R / 255.0f, pixel.G / 255.0f, pixel.B / 255.0f, pixel.A / 255.0f };
		}

		bool IsSimilar(Pixel a, Pixel b, int tolerance)
		{
			if (a.A == 0 && b.A == 0) {
				return true; // every fully transparent pixel counts as the same colour
			}
			return std::abs(a.R - b.R) <= tolerance && std::abs(a.G - b.G) <= tolerance
				&& std::abs(a.B - b.B) <= tolerance && std::abs(a.A - b.A) <= tolerance;
		}

		Image MakeChecker(int width, int height)
		{
			const int cell = std::max(1, std::min(width, height) / 16);
			Image image(width, height);
			for (int y = 0; y < height; ++y) {
				for (int x = 0; x < width; ++x) {
					const bool bDark = ((x / cell) + (y / cell)) % 2 != 0;
					const uint8_t value = bDark ? 150 : 200;
					image.At(x, y) = { value, value, value, 255 };
				}
			}
			return image;
		}

		std::string WithPngExtension(std::string path)
		{
			std::filesystem::path file = Utf8ToPath(path);
			if (PathToUtf8(file.extension()) != ".png") {
				file.replace_extension(".png");
			}
			return PathToGenericUtf8(file);
		}
	}

	bool DrawTexturePicker(EditorContext& context, char* search, size_t searchSize, Uid& outChosen)
	{
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##Search", "Search textures", search, searchSize);
		bool bChosen = false;
		if (ImGui::BeginChild("Textures", ImVec2(420.0f, 300.0f), ImGuiChildFlags_Borders)) {
			const std::string filter = search;
			for (const AssetInfo& asset : context.Assets.GetAssets()) {
				if (asset.Type != Texture::TypeName || asset.IsSubAsset()) {
					continue;
				}
				const std::string path = PathToGenericUtf8(asset.Path);
				if (!filter.empty() && path.find(filter) == std::string::npos) {
					continue;
				}
				ImGui::PushID(asset.Id.ToString().c_str());
				const ImVec2 rowMin = ImGui::GetCursorScreenPos();
				if (ImGui::Selectable("##Texture", false, 0, ImVec2(0.0f, 32.0f))) {
					outChosen = asset.Id;
					bChosen = true;
				}
				context.Thumbnails.Draw(ImGui::GetWindowDrawList(), asset.Id, rowMin, ImVec2(rowMin.x + 32.0f, rowMin.y + 32.0f));
				ImGui::GetWindowDrawList()->AddText(ImVec2(rowMin.x + 40.0f, rowMin.y + 8.0f), ImGui::GetColorU32(ImGuiCol_Text), path.c_str());
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		return bChosen;
	}

	PaintEditor::PaintEditor(EditorContext& context, SDL_Window* mainWindow, Uid paintingId)
		: AssetEditor("Painting", "Paint", paintingId)
		, Context{ context }
		, MainWindow{ mainWindow }
		, PaintingId{ paintingId }
	{
		Load();
	}

	PaintEditor::~PaintEditor() = default;

	bool PaintEditor::HasUnsavedChanges() const
	{
		return bIsLoaded && (bSettingsChanged || (HistoryIndex < History.size() && History[HistoryIndex].Revision != SavedRevision));
	}

	// -----------------------
	// Loading, saving, export
	// -----------------------

	void PaintEditor::Load()
	{
		const AssetInfo* asset = Context.Assets.Find(PaintingId);
		if (!asset) {
			Error = "This painting no longer exists.";
			return;
		}
		Error.clear();
		bIsLoaded = Doc.Load(Context.Assets.GetSourcePath(*asset), Error);
		if (!bIsLoaded) {
			return;
		}
		if (Doc.ExportPath.empty()) {
			std::filesystem::path exportPath = asset->Path;
			exportPath.replace_extension(".png");
			Doc.ExportPath = PathToGenericUtf8(exportPath);
		}
		std::snprintf(ExportPathBuffer, sizeof(ExportPathBuffer), "%s", Doc.ExportPath.c_str());
		CurrentLayer = static_cast<int>(Doc.Layers.size()) - 1;
		Selection = {};
		bSettingsChanged = false;
		ResetHistory();
		MarkAllDirty();
		bFitRequested = true;
		UpdateTitle();
	}

	bool PaintEditor::Save()
	{
		const AssetInfo* asset = Context.Assets.Find(PaintingId);
		if (!bIsLoaded || !asset) {
			return false;
		}
		const std::filesystem::path file = Context.Assets.GetSourcePath(*asset);
		if (Doc.bExportOnSave && !Export()) {
			return false; // asks first, or failed (Message says why); the painting itself isn't saved yet
		}
		std::string error;
		if (!Doc.Save(file, error)) {
			Message = error;
			return false;
		}
		SavedRevision = History[HistoryIndex].Revision;
		bSettingsChanged = false;
		Context.Thumbnails.Invalidate(PaintingId);
		if (!Doc.bExportOnSave) {
			Message = "Saved.";
		}
		UpdateTitle();
		return true;
	}

	bool PaintEditor::Export(bool bConfirmed)
	{
		AssetDatabase& database = Context.Assets;
		// The exported texture may have been moved or renamed: its Uid wins over the stored path.
		if (Doc.ExportTexture.IsValid()) {
			if (const AssetInfo* texture = database.Find(Doc.ExportTexture); texture && !texture->IsSubAsset()) {
				const std::string path = PathToGenericUtf8(texture->Path);
				if (path != Doc.ExportPath) {
					Doc.ExportPath = path;
					std::snprintf(ExportPathBuffer, sizeof(ExportPathBuffer), "%s", path.c_str());
					bSettingsChanged = true;
				}
			}
		}
		Doc.ExportPath = WithPngExtension(Doc.ExportPath);
		const std::filesystem::path target = database.GetContentDirectory() / Utf8ToPath(Doc.ExportPath);

		// Never overwrite a texture this painting didn't make without asking.
		const AssetInfo* existing = database.FindByPath(Utf8ToPath(Doc.ExportPath));
		std::error_code error;
		if (!bConfirmed && std::filesystem::exists(target, error) && (!existing || existing->Id != Doc.ExportTexture)) {
			bRequestOverwritePopup = true;
			return false;
		}

		std::filesystem::create_directories(target.parent_path(), error);
		const Image flat = Doc.Flatten();
		std::string saveError;
		if (!SavePngFile(flat, target, saveError)) {
			Message = saveError;
			return false;
		}

		Context.RefreshAssets(); // creates the .meta the first time; keeps the Uid afterwards
		const AssetInfo* texture = database.FindByPath(Utf8ToPath(Doc.ExportPath));
		if (!texture) {
			Message = "Exported, but " + Doc.ExportPath + " isn't a texture asset.";
			return false;
		}
		if (Doc.ExportTexture != texture->Id) {
			Doc.ExportTexture = texture->Id;
			bSettingsChanged = true;
		}
		// Everything using the texture (sprites, the scene, thumbnails) shows the new image at once.
		if (auto loaded = std::dynamic_pointer_cast<Texture>(AssetManager::FindLoaded(texture->Id))) {
			std::string updateError;
			if (!loaded->UpdatePixels(flat.Width, flat.Height, flat.Data(), updateError)) {
				Message = updateError;
				return false;
			}
		}
		Message = "Exported to " + Doc.ExportPath + ".";
		UpdateTitle();
		return true;
	}

	void PaintEditor::UpdateTitle()
	{
		const AssetInfo* asset = Context.Assets.Find(PaintingId);
		std::string title = asset ? asset->Name : "Painting";
		if (HasUnsavedChanges()) {
			title += "*";
		}
		if (title != LastTitle) {
			LastTitle = title;
			SetTitle(title);
		}
	}

	// -----------------------
	// Undo
	// -----------------------

	void PaintEditor::ResetHistory()
	{
		History.clear();
		HistoryIndex = 0;
		RecordUndo("Open");
		SavedRevision = History.back().Revision;
	}

	void PaintEditor::RecordUndo(const std::string& name)
	{
		if (!History.empty()) {
			History.resize(HistoryIndex + 1); // a new edit drops the redo states
		}
		History.push_back(HistoryState{ name, Doc.Width, Doc.Height, Doc.Layers, CurrentLayer, Selection, ++RevisionCounter });

		// Drop the oldest states beyond the count or the memory budget (pixels shared between states count once).
		const auto historyBytes = [this]() {
			std::unordered_set<const Image*> images;
			size_t bytes = 0;
			for (const HistoryState& state : History) {
				for (const Layer& layer : state.Layers) {
					if (images.insert(layer.Pixels.get()).second) {
						bytes += layer.Pixels->Pixels.size() * sizeof(Pixel);
					}
				}
			}
			return bytes;
		};
		while (History.size() > 2 && (History.size() > MAXHISTORY || historyBytes() > MAXHISTORYBYTES)) {
			History.erase(History.begin());
		}
		HistoryIndex = History.size() - 1;
		UpdateTitle();
	}

	void PaintEditor::RestoreState(const HistoryState& state)
	{
		Doc.Width = state.Width;
		Doc.Height = state.Height;
		Doc.Layers = state.Layers;
		CurrentLayer = std::clamp(state.CurrentLayer, 0, static_cast<int>(Doc.Layers.size()) - 1);
		Selection = state.Selection;
		MarkAllDirty();
		UpdateTitle();
	}

	void PaintEditor::Undo()
	{
		if (!bStroking && HistoryIndex > 0) {
			Message = "Undo " + History[HistoryIndex].Name;
			RestoreState(History[--HistoryIndex]);
		}
	}

	void PaintEditor::Redo()
	{
		if (!bStroking && HistoryIndex + 1 < History.size()) {
			RestoreState(History[++HistoryIndex]);
			Message = "Redo " + History[HistoryIndex].Name;
		}
	}

	// -----------------------
	// Window
	// -----------------------

	void PaintEditor::Draw()
	{
		ImGui::SetNextWindowSize(ImVec2(1200.0f, 760.0f), ImGuiCond_FirstUseEver);
		const bool bVisible = BeginWindow(ImGuiWindowFlags_MenuBar);

		// The tab's close button: keep the window open and ask when there are unsaved changes.
		if (!bIsOpen && HasUnsavedChanges()) {
			bIsOpen = true;
			bRequestUnsavedPopup = true;
		}
		if (!bVisible) {
			EndWindow();
			return;
		}
		if (!bIsLoaded) {
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", Error.c_str());
			if (ImGui::Button("Reload")) {
				Load();
			}
			EndWindow();
			return;
		}

		std::filesystem::path imported;
		if (ImportDialog.PollResult(imported)) {
			ImportLayerFromFile(imported);
		}

		HandleShortcuts();
		DrawMenuBar();
		UpdateDisplay();

		const float sidebarWidth = 290.0f;
		if (ImGui::BeginChild("Tools", ImVec2(96.0f, 0.0f), ImGuiChildFlags_Borders)) {
			DrawToolbar();
		}
		ImGui::EndChild();
		ImGui::SameLine();
		if (ImGui::BeginChild("Canvas", ImVec2(-sidebarWidth, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove)) {
			DrawCanvas();
		}
		ImGui::EndChild();
		ImGui::SameLine();
		if (ImGui::BeginChild("Sidebar", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
			DrawSidebar();
		}
		ImGui::EndChild();

		DrawPopups();
		EndWindow();
	}

	void PaintEditor::HandleShortcuts()
	{
		if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::GetIO().WantTextInput) {
			return;
		}
		// Focused routes win over the editor's global shortcuts (scene undo, Ctrl+S, Ctrl+D).
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z)) {
			Undo();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) {
			Redo();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) {
			Save();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_E)) {
			Export();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A)) {
			Selection = { 0, 0, Doc.Width, Doc.Height };
			RecordUndo("Select All");
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D) || ImGui::Shortcut(ImGuiKey_Escape)) {
			if (!Selection.IsEmpty()) {
				Selection = {};
				RecordUndo("Deselect");
			}
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C)) {
			CopySelection(false);
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_X)) {
			CopySelection(true);
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_V)) {
			Paste();
		}
		if (ImGui::Shortcut(ImGuiKey_Delete)) {
			ClearSelection();
		}
		if (bStroking) {
			return;
		}
		for (size_t index = 0; index < std::size(TOOLS); ++index) {
			if (ImGui::Shortcut(TOOLS[index].Key)) {
				CurrentTool = static_cast<Tool>(index);
			}
		}
		if (ImGui::Shortcut(ImGuiKey_X)) {
			std::swap(Primary, Secondary);
		}
		if (ImGui::Shortcut(ImGuiKey_F)) {
			bFitRequested = true;
		}
		int* size = CurrentTool == Tool::Brush ? &BrushSize : CurrentTool == Tool::Eraser ? &EraserSize : IsShapeTool() ? &ShapeWidth : &PencilSize;
		if (ImGui::Shortcut(ImGuiKey_LeftBracket, ImGuiInputFlags_Repeat)) {
			*size = std::max(1, *size - 1);
		}
		if (ImGui::Shortcut(ImGuiKey_RightBracket, ImGuiInputFlags_Repeat)) {
			*size = std::min(256, *size + 1);
		}
	}

	void PaintEditor::DrawMenuBar()
	{
		if (!ImGui::BeginMenuBar()) {
			return;
		}
		if (ImGui::BeginMenu("Painting")) {
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				Save();
			}
			if (ImGui::MenuItem("Export Texture", "Ctrl+E")) {
				Export();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Revert to Saved", nullptr, false, HasUnsavedChanges())) {
				Load();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem(HistoryIndex > 0 ? ("Undo " + History[HistoryIndex].Name).c_str() : "Undo", "Ctrl+Z", false, HistoryIndex > 0)) {
				Undo();
			}
			if (ImGui::MenuItem(HistoryIndex + 1 < History.size() ? ("Redo " + History[HistoryIndex + 1].Name).c_str() : "Redo", "Ctrl+Y", false, HistoryIndex + 1 < History.size())) {
				Redo();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "Ctrl+X")) {
				CopySelection(true);
			}
			if (ImGui::MenuItem("Copy", "Ctrl+C")) {
				CopySelection(false);
			}
			if (ImGui::MenuItem("Paste as New Layer", "Ctrl+V", false, Clipboard.Width > 0)) {
				Paste();
			}
			if (ImGui::MenuItem("Clear", "Del")) {
				ClearSelection();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Select All", "Ctrl+A")) {
				Selection = { 0, 0, Doc.Width, Doc.Height };
				RecordUndo("Select All");
			}
			if (ImGui::MenuItem("Deselect", "Ctrl+D", false, !Selection.IsEmpty())) {
				Selection = {};
				RecordUndo("Deselect");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Layer")) {
			if (ImGui::MenuItem("New Layer")) {
				AddLayer("Layer " + std::to_string(Doc.Layers.size() + 1), nullptr);
			}
			if (ImGui::MenuItem("Duplicate Layer")) {
				DuplicateLayer();
			}
			if (ImGui::MenuItem("Delete Layer", nullptr, false, Doc.Layers.size() > 1)) {
				DeleteLayer();
			}
			if (ImGui::MenuItem("Merge Down", nullptr, false, CurrentLayer > 0)) {
				MergeDown();
			}
			if (ImGui::MenuItem("Flatten", nullptr, false, Doc.Layers.size() > 1)) {
				Flatten();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Flip Layer Horizontally")) {
				FlipLayer(true);
			}
			if (ImGui::MenuItem("Flip Layer Vertically")) {
				FlipLayer(false);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Import Image File as Layer...", nullptr, false, !ImportDialog.IsOpen())) {
				ImportDialog.OpenFile(MainWindow, "Images", "png;bmp", Context.Assets.GetContentDirectory());
			}
			if (ImGui::MenuItem("Import Texture as Layer...")) {
				bRequestTexturePopup = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Canvas")) {
			if (ImGui::MenuItem("Canvas Size...")) {
				bRequestSizePopup = true;
				bSizePopupScales = false;
			}
			if (ImGui::MenuItem("Image Size...")) {
				bRequestSizePopup = true;
				bSizePopupScales = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Flip Horizontally")) {
				FlipCanvas(true);
			}
			if (ImGui::MenuItem("Flip Vertically")) {
				FlipCanvas(false);
			}
			if (ImGui::MenuItem("Rotate 90\xC2\xB0 Clockwise")) {
				RotateCanvas(true);
			}
			if (ImGui::MenuItem("Rotate 90\xC2\xB0 Counter-clockwise")) {
				RotateCanvas(false);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View")) {
			ImGui::MenuItem("Pixel Grid", nullptr, &bShowGrid);
			if (ImGui::MenuItem("Fit", "F")) {
				bFitRequested = true;
			}
			if (ImGui::MenuItem("Actual Size")) {
				Zoom = 1.0f;
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();
		ImGui::Checkbox("Mirror X", &bMirrorX);
		ImGui::Checkbox("Mirror Y", &bMirrorY);
		if (!Message.empty()) {
			ImGui::Separator();
			ImGui::TextDisabled("%s", Message.c_str());
		}
		ImGui::EndMenuBar();
	}

	void PaintEditor::DrawToolbar()
	{
		const float width = ImGui::GetContentRegionAvail().x;
		for (size_t index = 0; index < std::size(TOOLS); ++index) {
			const bool bActive = CurrentTool == static_cast<Tool>(index);
			if (bActive) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			}
			if (ImGui::Button(TOOLS[index].Name, ImVec2(width, 0.0f)) && !bStroking) {
				CurrentTool = static_cast<Tool>(index);
			}
			if (bActive) {
				ImGui::PopStyleColor();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s (%s)", TOOLS[index].Name, TOOLS[index].Shortcut);
			}
			if (index == 4 || index == 7) {
				ImGui::Separator();
			}
		}

		// Primary over secondary, like most paint programs. X swaps them.
		ImGui::Separator();
		ImGui::ColorButton("##Primary", ToImVec4(Primary), ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2(width * 0.65f, 28.0f));
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width * 0.35f);
		ImGui::ColorButton("##Secondary", ToImVec4(Secondary), ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2(width * 0.65f, 28.0f));
		if (ImGui::Button("Swap (X)", ImVec2(width, 0.0f))) {
			std::swap(Primary, Secondary);
		}
	}

	void PaintEditor::FitToView(float width, float height)
	{
		const float fit = std::min(width / Doc.Width, height / Doc.Height) * 0.9f;
		// Whole steps above 1x keep pixels square.
		Zoom = std::clamp(fit >= 1.0f ? std::floor(fit) : fit, MINZOOM, MAXZOOM);
		PanX = (width - Doc.Width * Zoom) * 0.5f;
		PanY = (height - Doc.Height * Zoom) * 0.5f;
	}

	void PaintEditor::UpdateDisplay()
	{
		const bool bSizeChanged = CompositeImage.Width != Doc.Width || CompositeImage.Height != Doc.Height;
		if (bSizeChanged) {
			CompositeImage = Image(Doc.Width, Doc.Height);
			CompositeDirty = CompositeImage.GetRect();
			const Image checker = MakeChecker(Doc.Width, Doc.Height);
			std::string error;
			if (CheckerTexture) {
				CheckerTexture->UpdatePixels(checker.Width, checker.Height, checker.Data(), error);
			} else {
				CheckerTexture = Texture::CreateFromPixels(checker.Width, checker.Height, checker.Data(), TextureFilter::Nearest, error);
			}
		}
		if (CompositeDirty.IsEmpty()) {
			return;
		}
		Doc.Composite(CompositeImage, CompositeDirty);
		CompositeDirty = {};
		std::string error;
		if (DisplayTexture) {
			DisplayTexture->UpdatePixels(CompositeImage.Width, CompositeImage.Height, CompositeImage.Data(), error);
		} else {
			DisplayTexture = Texture::CreateFromPixels(CompositeImage.Width, CompositeImage.Height, CompositeImage.Data(), TextureFilter::Nearest, error);
		}
		if (!error.empty()) {
			Message = error;
		}
	}

	void PaintEditor::DrawCanvas()
	{
		ImGuiIO& io = ImGui::GetIO();
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 size = ImGui::GetContentRegionAvail();
		if (size.x <= 1.0f || size.y <= 1.0f) {
			return;
		}
		if (bFitRequested) {
			FitToView(size.x, size.y);
			bFitRequested = false;
		}

		ImGui::InvisibleButton("CanvasInput", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
		const bool bHovered = ImGui::IsItemHovered();
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// Zoom around the mouse.
		if (bHovered && io.MouseWheel != 0.0f) {
			const float canvasX = (io.MousePos.x - origin.x - PanX) / Zoom;
			const float canvasY = (io.MousePos.y - origin.y - PanY) / Zoom;
			float newZoom = io.MouseWheel > 0.0f ? Zoom * 1.25f : Zoom / 1.25f;
			if (newZoom > 1.0f) {
				newZoom = io.MouseWheel > 0.0f ? std::ceil(newZoom) : std::floor(newZoom);
			}
			Zoom = std::clamp(newZoom, MINZOOM, MAXZOOM);
			PanX = io.MousePos.x - origin.x - canvasX * Zoom;
			PanY = io.MousePos.y - origin.y - canvasY * Zoom;
		}

		// Pan: middle mouse, or Space + left mouse.
		const bool bSpacePan = ImGui::IsKeyDown(ImGuiKey_Space) && !io.WantTextInput;
		if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) || (bSpacePan && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)))) {
			PanX += io.MouseDelta.x;
			PanY += io.MouseDelta.y;
		}

		const ImVec2 imageMin{ origin.x + PanX, origin.y + PanY };
		const ImVec2 imageMax{ imageMin.x + Doc.Width * Zoom, imageMin.y + Doc.Height * Zoom };
		drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(45, 45, 48, 255));
		// Both textures use the Nearest filter, so the pixels stay sharp.
		if (CheckerTexture) {
			drawList->AddImage(ToImTexture(CheckerTexture), imageMin, imageMax);
		}
		if (DisplayTexture) {
			drawList->AddImage(ToImTexture(DisplayTexture), imageMin, imageMax);
		}

		// Pixel grid from 8x zoom, only the visible lines.
		if (bShowGrid && Zoom >= 8.0f) {
			const int firstX = std::max(0, static_cast<int>((origin.x - imageMin.x) / Zoom));
			const int lastX = std::min(Doc.Width, static_cast<int>((origin.x + size.x - imageMin.x) / Zoom) + 1);
			const int firstY = std::max(0, static_cast<int>((origin.y - imageMin.y) / Zoom));
			const int lastY = std::min(Doc.Height, static_cast<int>((origin.y + size.y - imageMin.y) / Zoom) + 1);
			const ImU32 gridColor = IM_COL32(255, 255, 255, 36);
			for (int x = firstX; x <= lastX; ++x) {
				drawList->AddLine(ImVec2(imageMin.x + x * Zoom, imageMin.y + firstY * Zoom), ImVec2(imageMin.x + x * Zoom, imageMin.y + lastY * Zoom), gridColor);
			}
			for (int y = firstY; y <= lastY; ++y) {
				drawList->AddLine(ImVec2(imageMin.x + firstX * Zoom, imageMin.y + y * Zoom), ImVec2(imageMin.x + lastX * Zoom, imageMin.y + y * Zoom), gridColor);
			}
		}
		drawList->AddRect(ImVec2(imageMin.x - 1.0f, imageMin.y - 1.0f), ImVec2(imageMax.x + 1.0f, imageMax.y + 1.0f), IM_COL32(0, 0, 0, 255));

		// Mirror axes
		const ImU32 axisColor = IM_COL32(80, 200, 255, 160);
		if (bMirrorX) {
			const float x = imageMin.x + Doc.Width * 0.5f * Zoom;
			drawList->AddLine(ImVec2(x, imageMin.y), ImVec2(x, imageMax.y), axisColor);
		}
		if (bMirrorY) {
			const float y = imageMin.y + Doc.Height * 0.5f * Zoom;
			drawList->AddLine(ImVec2(imageMin.x, y), ImVec2(imageMax.x, y), axisColor);
		}

		// Selection
		if (!Selection.IsEmpty()) {
			const ImVec2 min{ imageMin.x + Selection.X * Zoom, imageMin.y + Selection.Y * Zoom };
			const ImVec2 max{ imageMin.x + Selection.Right() * Zoom, imageMin.y + Selection.Bottom() * Zoom };
			drawList->AddRect(min, max, IM_COL32(0, 0, 0, 255), 0.0f, 0, 3.0f);
			drawList->AddRect(min, max, IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.0f);
		}

		// Mouse position in canvas pixels
		const float mouseCanvasX = (io.MousePos.x - imageMin.x) / Zoom;
		const float mouseCanvasY = (io.MousePos.y - imageMin.y) / Zoom;
		bHasMouse = bHovered;
		MouseX = static_cast<int>(std::floor(mouseCanvasX));
		MouseY = static_cast<int>(std::floor(mouseCanvasY));

		// Tools
		if (bHovered && !bSpacePan && !bStroking) {
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				BeginStroke(mouseCanvasX, mouseCanvasY, false);
			} else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				BeginStroke(mouseCanvasX, mouseCanvasY, true);
			}
		} else if (bStroking) {
			const ImGuiMouseButton button = bStrokeSecondary ? ImGuiMouseButton_Right : ImGuiMouseButton_Left;
			if (ImGui::IsMouseDown(button)) {
				if (mouseCanvasX != StrokeLastX || mouseCanvasY != StrokeLastY) {
					ContinueStroke(mouseCanvasX, mouseCanvasY);
				}
			} else {
				EndStroke();
			}
		}

		// Brush outline under the mouse
		if (bHovered && (CurrentTool == Tool::Pencil || CurrentTool == Tool::Brush || CurrentTool == Tool::Eraser)) {
			const int brushSize = CurrentTool == Tool::Brush ? BrushSize : CurrentTool == Tool::Eraser ? EraserSize : PencilSize;
			if (CurrentTool == Tool::Pencil) {
				const float half = brushSize * 0.5f;
				const float centreX = brushSize % 2 != 0 ? std::floor(mouseCanvasX) + 0.5f : std::round(mouseCanvasX);
				const float centreY = brushSize % 2 != 0 ? std::floor(mouseCanvasY) + 0.5f : std::round(mouseCanvasY);
				drawList->AddRect(ImVec2(imageMin.x + (centreX - half) * Zoom, imageMin.y + (centreY - half) * Zoom),
					ImVec2(imageMin.x + (centreX + half) * Zoom, imageMin.y + (centreY + half) * Zoom), IM_COL32(255, 255, 255, 180));
			} else {
				drawList->AddCircle(io.MousePos, std::max(1.0f, brushSize * 0.5f * Zoom), IM_COL32(255, 255, 255, 180), 32);
			}
		}

		// Status
		char status[128];
		std::snprintf(status, sizeof(status), "%d x %d  |  %.0f%%%s", Doc.Width, Doc.Height, Zoom * 100.0f, "");
		std::string statusText = status;
		if (bHovered && MouseX >= 0 && MouseY >= 0 && MouseX < Doc.Width && MouseY < Doc.Height) {
			statusText += "  |  " + std::to_string(MouseX) + ", " + std::to_string(MouseY);
		}
		if (!Selection.IsEmpty()) {
			statusText += "  |  selection " + std::to_string(Selection.Width) + " x " + std::to_string(Selection.Height);
		}
		drawList->AddText(ImVec2(origin.x + 6.0f, origin.y + 4.0f), IM_COL32(220, 220, 220, 255), statusText.c_str());
	}

	void PaintEditor::DrawSidebar()
	{
		if (ImGui::CollapsingHeader("Colours", ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawColors();
		}
		if (ImGui::CollapsingHeader("Tool", ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawToolOptions();
		}
		if (ImGui::CollapsingHeader("Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawLayers();
		}
		if (ImGui::CollapsingHeader("Export", ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawExport();
		}
	}

	void PaintEditor::DrawToolOptions()
	{
		ImGui::PushItemWidth(-90.0f);
		switch (CurrentTool)
		{
		case Tool::Pencil:
			ImGui::SliderInt("Size", &PencilSize, 1, 64);
			Hint("Hard pixels. Right mouse paints the secondary colour.");
			break;
		case Tool::Brush:
			ImGui::SliderInt("Size", &BrushSize, 1, 256);
			ImGui::SliderFloat("Hardness", &BrushHardness, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Opacity", &BrushOpacity, 0.01f, 1.0f, "%.2f");
			ImGui::SliderFloat("Spacing", &BrushSpacing, 0.02f, 1.0f, "%.2f");
			break;
		case Tool::Eraser:
			ImGui::SliderInt("Size", &EraserSize, 1, 256);
			ImGui::SliderFloat("Hardness", &EraserHardness, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Opacity", &EraserOpacity, 0.01f, 1.0f, "%.2f");
			break;
		case Tool::Fill:
			ImGui::SliderInt("Tolerance", &FillTolerance, 0, 255);
			ImGui::Checkbox("Contiguous", &bFillContiguous);
			ImGui::Checkbox("Sample all layers", &bSampleAllLayers);
			break;
		case Tool::Picker:
			ImGui::Checkbox("Sample all layers", &bSampleAllLayers);
			Hint("Left: primary, right: secondary. Alt+click picks with any tool.");
			break;
		case Tool::Line:
		case Tool::Rectangle:
		case Tool::Ellipse:
			ImGui::SliderInt("Width", &ShapeWidth, 1, 64);
			if (CurrentTool != Tool::Line) {
				ImGui::Checkbox("Filled", &bShapeFilled);
			}
			Hint(CurrentTool == Tool::Line ? "Shift: 45\xC2\xB0 steps." : "Shift: square / circle.");
			break;
		case Tool::Select:
			Hint("Drag to select; drag inside the selection to move its pixels. Painting stays inside it.");
			break;
		case Tool::Move:
			Hint("Moves the selection's pixels, or the whole layer without one.");
			break;
		}
		ImGui::PopItemWidth();
	}

	void PaintEditor::DrawColors()
	{
		float primary[4];
		float secondary[4];
		ToFloat4(Primary, primary);
		ToFloat4(Secondary, secondary);
		ImGui::PushItemWidth(-90.0f);
		if (ImGui::ColorEdit4("Primary", primary, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
			Primary = FromFloat4(primary);
		}
		if (ImGui::ColorEdit4("Secondary", secondary, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
			Secondary = FromFloat4(secondary);
		}
		ImGui::PopItemWidth();

		// Palette: click = primary, right-click = secondary, Shift+click removes.
		const float swatch = 20.0f;
		const int perRow = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + 4.0f) / (swatch + 4.0f)));
		int removeIndex = -1;
		for (int index = 0; index < static_cast<int>(Doc.Palette.size()); ++index) {
			ImGui::PushID(index);
			if (index % perRow != 0) {
				ImGui::SameLine(0.0f, 4.0f);
			}
			if (ImGui::ColorButton("##Swatch", ToImVec4(Doc.Palette[index]), ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip, ImVec2(swatch, swatch))) {
				if (ImGui::GetIO().KeyShift) {
					removeIndex = index;
				} else {
					Primary = Doc.Palette[index];
				}
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
				Secondary = Doc.Palette[index];
			}
			ImGui::PopID();
		}
		if (removeIndex >= 0) {
			Doc.Palette.erase(Doc.Palette.begin() + removeIndex);
			bSettingsChanged = true;
		}
		if (ImGui::SmallButton("+ Add primary")) {
			Doc.Palette.push_back(Primary);
			bSettingsChanged = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Default palette")) {
			Doc.Palette = Painting::DefaultPalette();
			bSettingsChanged = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Shift+click a colour to remove it.");
		}
		UpdateTitle();
	}

	void PaintEditor::DrawLayers()
	{
		// Top layer first, like other paint programs.
		int moveFrom = -1;
		int moveTo = -1;
		if (ImGui::BeginChild("LayerList", ImVec2(0.0f, std::min(200.0f, 26.0f * static_cast<float>(Doc.Layers.size()) + 8.0f)), ImGuiChildFlags_Borders)) {
			for (int index = static_cast<int>(Doc.Layers.size()) - 1; index >= 0; --index) {
				Layer& layer = Doc.Layers[index];
				ImGui::PushID(index);
				bool bVisible = layer.bIsVisible;
				if (ImGui::Checkbox("##Visible", &bVisible)) {
					layer.bIsVisible = bVisible;
					MarkAllDirty();
					RecordUndo(bVisible ? "Show Layer" : "Hide Layer");
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Visible");
				}
				ImGui::SameLine();
				if (ImGui::SmallButton(layer.bIsLocked ? "L" : "-")) {
					layer.bIsLocked = !layer.bIsLocked;
					RecordUndo(layer.bIsLocked ? "Lock Layer" : "Unlock Layer");
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(layer.bIsLocked ? "Locked (click to unlock)" : "Unlocked (click to lock)");
				}
				ImGui::SameLine();
				std::string label = layer.Name;
				if (layer.Blend != BlendMode::Normal || layer.Opacity < 1.0f) {
					label += "  (" + std::string(ToString(layer.Blend)) + " " + std::to_string(static_cast<int>(layer.Opacity * 100.0f + 0.5f)) + "%)";
				}
				if (ImGui::Selectable(label.c_str(), CurrentLayer == index) && !bStroking) {
					CurrentLayer = index;
				}
				// Drag a layer onto another to move it there.
				if (ImGui::BeginDragDropSource()) {
					ImGui::SetDragDropPayload("FLEX_PAINT_LAYER", &index, sizeof(int));
					ImGui::TextUnformatted(layer.Name.c_str());
					ImGui::EndDragDropSource();
				}
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FLEX_PAINT_LAYER")) {
						moveFrom = *static_cast<const int*>(payload->Data);
						moveTo = index;
					}
					ImGui::EndDragDropTarget();
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		if (moveFrom >= 0 && moveFrom != moveTo) {
			MoveLayer(moveFrom, moveTo);
		}

		if (ImGui::Button("New")) {
			AddLayer("Layer " + std::to_string(Doc.Layers.size() + 1), nullptr);
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy")) {
			DuplicateLayer();
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(Doc.Layers.size() <= 1);
		if (ImGui::Button("Delete")) {
			DeleteLayer();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(CurrentLayer + 1 >= static_cast<int>(Doc.Layers.size()));
		if (ImGui::ArrowButton("##Up", ImGuiDir_Up)) {
			MoveLayer(CurrentLayer, CurrentLayer + 1);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(CurrentLayer <= 0);
		if (ImGui::ArrowButton("##Down", ImGuiDir_Down)) {
			MoveLayer(CurrentLayer, CurrentLayer - 1);
		}
		ImGui::SameLine();
		if (ImGui::Button("Merge")) {
			MergeDown();
		}
		ImGui::EndDisabled();

		// The current layer's settings: one undo step per finished edit.
		Layer& layer = Doc.Layers[CurrentLayer];
		ImGui::PushItemWidth(-90.0f);
		char name[128];
		std::snprintf(name, sizeof(name), "%s", layer.Name.c_str());
		if (ImGui::InputText("Name", name, sizeof(name))) {
			layer.Name = name;
		}
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			RecordUndo("Rename Layer");
		}
		float opacity = layer.Opacity * 100.0f;
		if (ImGui::SliderFloat("Opacity", &opacity, 0.0f, 100.0f, "%.0f%%")) {
			layer.Opacity = std::clamp(opacity / 100.0f, 0.0f, 1.0f);
			MarkAllDirty();
		}
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			RecordUndo("Layer Opacity");
		}
		if (ImGui::BeginCombo("Blend", ToString(layer.Blend))) {
			for (const BlendMode mode : AllBlendModes) {
				if (ImGui::Selectable(ToString(mode), layer.Blend == mode) && layer.Blend != mode) {
					layer.Blend = mode;
					MarkAllDirty();
					RecordUndo("Blend Mode");
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
	}

	void PaintEditor::DrawExport()
	{
		ImGui::TextWrapped("Exports the visible layers as a texture (.png) the game uses. The painting itself never goes into builds.");
		ImGui::PushItemWidth(-90.0f);
		if (ImGui::InputText("Texture", ExportPathBuffer, sizeof(ExportPathBuffer))) {
			Doc.ExportPath = ExportPathBuffer;
			Doc.ExportTexture = {}; // a new target: found again (or created) on the next export
			bSettingsChanged = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Path relative to Content/.");
		}
		ImGui::PopItemWidth();
		if (ImGui::Checkbox("Export when saving", &Doc.bExportOnSave)) {
			bSettingsChanged = true;
		}
		if (const AssetInfo* texture = Doc.ExportTexture.IsValid() ? Context.Assets.Find(Doc.ExportTexture) : nullptr) {
			ImGui::TextDisabled("Texture %s", texture->Id.ToString().c_str());
		} else {
			ImGui::TextDisabled("Not exported yet.");
		}
		if (ImGui::Button("Export Now (Ctrl+E)")) {
			Export();
		}
		ImGui::SameLine();
		if (ImGui::Button("Save (Ctrl+S)")) {
			Save();
		}
	}

	void PaintEditor::DrawPopups()
	{
		if (bRequestUnsavedPopup) {
			ImGui::OpenPopup(UNSAVEDPOPUP);
			bRequestUnsavedPopup = false;
		}
		if (bRequestOverwritePopup) {
			ImGui::OpenPopup(OVERWRITEPOPUP);
			bRequestOverwritePopup = false;
		}
		if (bRequestSizePopup) {
			ImGui::OpenPopup(SIZEPOPUP);
			NewWidth = Doc.Width;
			NewHeight = Doc.Height;
			bRequestSizePopup = false;
		}
		if (bRequestTexturePopup) {
			ImGui::OpenPopup(TEXTUREPOPUP);
			bRequestTexturePopup = false;
		}

		if (ImGui::BeginPopupModal(UNSAVEDPOPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::TextUnformatted("This painting has unsaved changes.");
			if (ImGui::Button("Save and Close")) {
				if (Save()) {
					bIsOpen = false;
				}
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Discard and Close")) {
				bIsLoaded = false; // nothing to save any more
				bIsOpen = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopupModal(OVERWRITEPOPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Content/%s already exists and wasn't exported from this painting.", Doc.ExportPath.c_str());
			ImGui::TextUnformatted("Overwrite it? Sprites and references to it keep working.");
			if (ImGui::Button("Overwrite")) {
				if (Export(true) && Doc.bExportOnSave) {
					Save();
				}
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopupModal(SIZEPOPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::TextUnformatted(bSizePopupScales ? "Image size: scales every layer (nearest neighbour)." : "Canvas size: adds or crops pixels at the edges.");
			ImGui::SetNextItemWidth(160.0f);
			ImGui::InputInt("Width", &NewWidth);
			ImGui::SetNextItemWidth(160.0f);
			ImGui::InputInt("Height", &NewHeight);
			NewWidth = std::clamp(NewWidth, 1, Painting::MaxSize);
			NewHeight = std::clamp(NewHeight, 1, Painting::MaxSize);
			if (!bSizePopupScales) {
				ImGui::Checkbox("Keep the image centred", &bAnchorCentre);
			}
			if (ImGui::Button("Apply")) {
				ResizeCanvas(NewWidth, NewHeight, bSizePopupScales, bAnchorCentre);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopupModal(TEXTUREPOPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			Uid chosen;
			if (DrawTexturePicker(Context, TextureSearch, sizeof(TextureSearch), chosen)) {
				ImportLayerFromTexture(chosen);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	// -----------------------
	// Painting
	// -----------------------

	PixelRect PaintEditor::GetPaintArea() const
	{
		const PixelRect canvas{ 0, 0, Doc.Width, Doc.Height };
		return Selection.IsEmpty() ? canvas : Selection.Intersect(canvas);
	}

	void PaintEditor::MarkDirty(const PixelRect& rect)
	{
		CompositeDirty = CompositeDirty.Union(rect.Intersect({ 0, 0, Doc.Width, Doc.Height }));
	}

	void PaintEditor::MarkAllDirty()
	{
		CompositeDirty = { 0, 0, Doc.Width, Doc.Height };
	}

	bool PaintEditor::CanPaintOnLayer()
	{
		const Layer& layer = Doc.Layers[CurrentLayer];
		if (layer.bIsLocked) {
			Message = "The layer \"" + layer.Name + "\" is locked.";
			return false;
		}
		if (!layer.bIsVisible) {
			Message = "The layer \"" + layer.Name + "\" is hidden.";
			return false;
		}
		return true;
	}

	void PaintEditor::BeginStroke(float x, float y, bool bSecondary)
	{
		const int pixelX = static_cast<int>(std::floor(x));
		const int pixelY = static_cast<int>(std::floor(y));
		const bool bPaintTool = CurrentTool == Tool::Pencil || CurrentTool == Tool::Brush || CurrentTool == Tool::Fill || IsShapeTool();
		if (CurrentTool == Tool::Picker || (bPaintTool && ImGui::GetIO().KeyAlt)) {
			PickColor(pixelX, pixelY, bSecondary);
			return;
		}

		StrokeStartX = StrokeLastX = StrokeEndX = x;
		StrokeStartY = StrokeLastY = StrokeEndY = y;
		bStrokeSecondary = bSecondary;
		Message.clear();

		if (CurrentTool == Tool::Select) {
			if (bSecondary) {
				return;
			}
			// Dragging inside the selection moves its pixels; anywhere else starts a new selection.
			if (!Selection.IsEmpty() && Selection.Contains(pixelX, pixelY)) {
				if (!CanPaintOnLayer()) {
					return;
				}
				bSelecting = false;
			} else {
				bSelecting = true;
				bStroking = true;
				Selection = {};
				return;
			}
		}
		if (bSecondary && (CurrentTool == Tool::Move || CurrentTool == Tool::Select)) {
			return;
		}
		if (!CanPaintOnLayer()) {
			return;
		}

		bStroking = true;
		StrokeBase = Doc.Layers[CurrentLayer].Pixels; // shared with the undo history...
		Doc.EditPixels(CurrentLayer);                  // ...so painting works on a copy
		StrokeMask.assign(static_cast<size_t>(Doc.Width) * Doc.Height, 0.0f);
		StrokeDirty = {};
		PendingDirty = {};

		if (CurrentTool == Tool::Move || CurrentTool == Tool::Select) {
			MoveSource = GetPaintArea();
			MoveLastDest = MoveSource;
		} else if (CurrentTool == Tool::Fill) {
			FloodFill(pixelX, pixelY);
			EndStroke();
		} else if (IsShapeTool()) {
			StampShape();
		} else {
			StampDab(x, y);
			ApplyStroke(PendingDirty);
			PendingDirty = {};
		}
	}

	void PaintEditor::ContinueStroke(float x, float y)
	{
		StrokeEndX = x;
		StrokeEndY = y;
		if (bSelecting) {
			const int x0 = std::clamp(static_cast<int>(std::floor(StrokeStartX)), 0, Doc.Width - 1);
			const int y0 = std::clamp(static_cast<int>(std::floor(StrokeStartY)), 0, Doc.Height - 1);
			const int x1 = std::clamp(static_cast<int>(std::floor(x)), 0, Doc.Width - 1);
			const int y1 = std::clamp(static_cast<int>(std::floor(y)), 0, Doc.Height - 1);
			Selection = PixelRect::FromCorners(std::min(x0, x1), std::min(y0, y1), std::max(x0, x1) + 1, std::max(y0, y1) + 1);
		} else if (CurrentTool == Tool::Move || CurrentTool == Tool::Select) {
			UpdateMove(static_cast<int>(std::round(x - StrokeStartX)), static_cast<int>(std::round(y - StrokeStartY)));
		} else if (IsShapeTool()) {
			StampShape();
		} else if (CurrentTool == Tool::Pencil || CurrentTool == Tool::Brush || CurrentTool == Tool::Eraser) {
			StampLine(StrokeLastX, StrokeLastY, x, y);
			ApplyStroke(PendingDirty); // only where the new dabs landed
			PendingDirty = {};
		}
		StrokeLastX = x;
		StrokeLastY = y;
	}

	void PaintEditor::EndStroke()
	{
		if (!bStroking) {
			return;
		}
		bStroking = false;
		if (bSelecting) {
			bSelecting = false;
			if (Selection != History[HistoryIndex].Selection) {
				RecordUndo(Selection.IsEmpty() ? "Deselect" : "Select");
			}
			return;
		}
		StrokeBase.reset();
		StrokeMask.clear();
		StrokeMask.shrink_to_fit();
		static constexpr const char* NAMES[] = { "Pencil", "Brush", "Eraser", "Fill", "Pick", "Line", "Rectangle", "Ellipse", "Move Selection", "Move" };
		RecordUndo(NAMES[static_cast<size_t>(CurrentTool)]);
	}

	void PaintEditor::StampDab(float x, float y)
	{
		const bool bHard = CurrentTool == Tool::Pencil || IsShapeTool();
		const int size = CurrentTool == Tool::Brush ? BrushSize : CurrentTool == Tool::Eraser ? EraserSize : IsShapeTool() ? ShapeWidth : PencilSize;
		const float hardness = CurrentTool == Tool::Brush ? BrushHardness : CurrentTool == Tool::Eraser ? EraserHardness : 1.0f;
		const bool bEraserHard = CurrentTool == Tool::Eraser && EraserHardness >= 1.0f;
		if (bHard || bEraserHard) {
			// Hard dabs sit on the pixel grid: odd sizes centre on a pixel, even sizes on a corner.
			x = size % 2 != 0 ? std::floor(x) + 0.5f : std::round(x);
			y = size % 2 != 0 ? std::floor(y) + 0.5f : std::round(y);
		}
		const bool bHardDab = bHard || bEraserHard;
		StampDabAt(x, y, size, hardness, bHardDab);
		if (bMirrorX) {
			StampDabAt(Doc.Width - x, y, size, hardness, bHardDab);
		}
		if (bMirrorY) {
			StampDabAt(x, Doc.Height - y, size, hardness, bHardDab);
		}
		if (bMirrorX && bMirrorY) {
			StampDabAt(Doc.Width - x, Doc.Height - y, size, hardness, bHardDab);
		}
	}

	void PaintEditor::StampDabAt(float x, float y, int size, float hardness, bool bHard)
	{
		const float radius = size * 0.5f;
		const PixelRect area = PixelRect{ static_cast<int>(std::floor(x - radius)), static_cast<int>(std::floor(y - radius)),
			static_cast<int>(std::ceil(x + radius)) - static_cast<int>(std::floor(x - radius)),
			static_cast<int>(std::ceil(y + radius)) - static_cast<int>(std::floor(y - radius)) }.Intersect(GetPaintArea());
		const float inner = radius * hardness;
		for (int py = area.Y; py < area.Bottom(); ++py) {
			for (int px = area.X; px < area.Right(); ++px) {
				const float dx = px + 0.5f - x;
				const float dy = py + 0.5f - y;
				const float distance = std::sqrt(dx * dx + dy * dy);
				float coverage;
				if (bHard) {
					// Square-ish for small sizes (a 2px pencil fills 2x2), round for bigger ones.
					coverage = (size <= 2 ? std::max(std::abs(dx), std::abs(dy)) < radius : distance <= radius + 0.05f) ? 1.0f : 0.0f;
				} else if (distance <= inner) {
					coverage = 1.0f;
				} else if (distance >= radius) {
					coverage = 0.0f;
				} else {
					const float t = 1.0f - (distance - inner) / std::max(0.0001f, radius - inner);
					coverage = t * t * (3.0f - 2.0f * t);
				}
				if (coverage > 0.0f) {
					float& mask = StrokeMask[static_cast<size_t>(py) * Doc.Width + px];
					mask = std::max(mask, coverage);
				}
			}
		}
		StrokeDirty = StrokeDirty.Union(area);
		PendingDirty = PendingDirty.Union(area);
	}

	void PaintEditor::StampLine(float x0, float y0, float x1, float y1)
	{
		const int size = CurrentTool == Tool::Brush ? BrushSize : CurrentTool == Tool::Eraser ? EraserSize : IsShapeTool() ? ShapeWidth : PencilSize;
		const float spacing = CurrentTool == Tool::Brush ? std::max(0.5f, size * BrushSpacing) : CurrentTool == Tool::Eraser ? std::max(0.5f, size * 0.15f) : 0.5f;
		const float length = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
		const int steps = std::max(1, static_cast<int>(std::ceil(length / spacing)));
		for (int step = 1; step <= steps; ++step) {
			const float t = static_cast<float>(step) / steps;
			StampDab(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
		}
	}

	void PaintEditor::StampShape()
	{
		// Shapes are redrawn from the stroke base on every mouse move.
		const PixelRect previous = StrokeDirty;
		ClearMask(previous);
		StrokeDirty = {};

		float x0 = StrokeStartX;
		float y0 = StrokeStartY;
		float x1 = StrokeEndX;
		float y1 = StrokeEndY;
		const bool bShift = ImGui::GetIO().KeyShift;
		if (CurrentTool == Tool::Line) {
			if (bShift) {
				// Snap to 45-degree steps.
				const float angle = std::atan2(y1 - y0, x1 - x0);
				const float snapped = std::round(angle / (3.14159265f / 4.0f)) * (3.14159265f / 4.0f);
				const float length = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
				x1 = x0 + std::cos(snapped) * length;
				y1 = y0 + std::sin(snapped) * length;
			}
			StampDab(x0, y0);
			StampLine(x0, y0, x1, y1);
		} else {
			if (bShift) {
				const float side = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
				x1 = x0 + (x1 >= x0 ? side : -side);
				y1 = y0 + (y1 >= y0 ? side : -side);
			}
			// Whole pixels: the shape covers the pixels from the start pixel to the end pixel.
			const int left = static_cast<int>(std::floor(std::min(x0, x1)));
			const int top = static_cast<int>(std::floor(std::min(y0, y1)));
			const int right = static_cast<int>(std::floor(std::max(x0, x1))) + 1;
			const int bottom = static_cast<int>(std::floor(std::max(y0, y1))) + 1;
			const PixelRect bounds{ left, top, right - left, bottom - top };
			const PixelRect area = GetPaintArea();
			const float centreX = (left + right) * 0.5f;
			const float centreY = (top + bottom) * 0.5f;
			const float radiusX = (right - left) * 0.5f;
			const float radiusY = (bottom - top) * 0.5f;
			const float width = static_cast<float>(ShapeWidth);
			const auto insideEllipse = [&](float px, float py, float shrink) {
				const float rx = radiusX - shrink;
				const float ry = radiusY - shrink;
				if (rx <= 0.0f || ry <= 0.0f) {
					return false;
				}
				const float nx = (px - centreX) / rx;
				const float ny = (py - centreY) / ry;
				return nx * nx + ny * ny <= 1.0f;
			};
			const auto mark = [&](int px, int py) {
				const auto markOne = [&](int mx, int my) {
					if (area.Contains(mx, my)) {
						StrokeMask[static_cast<size_t>(my) * Doc.Width + mx] = 1.0f;
						StrokeDirty = StrokeDirty.Union({ mx, my, 1, 1 });
					}
				};
				markOne(px, py);
				if (bMirrorX) {
					markOne(Doc.Width - 1 - px, py);
				}
				if (bMirrorY) {
					markOne(px, Doc.Height - 1 - py);
				}
				if (bMirrorX && bMirrorY) {
					markOne(Doc.Width - 1 - px, Doc.Height - 1 - py);
				}
			};
			for (int py = bounds.Y; py < bounds.Bottom(); ++py) {
				for (int px = bounds.X; px < bounds.Right(); ++px) {
					bool bInside;
					if (CurrentTool == Tool::Rectangle) {
						const bool bInnerRect = px >= left + ShapeWidth && px < right - ShapeWidth && py >= top + ShapeWidth && py < bottom - ShapeWidth;
						bInside = bShapeFilled || !bInnerRect;
					} else {
						const float cx = px + 0.5f;
						const float cy = py + 0.5f;
						bInside = insideEllipse(cx, cy, 0.0f) && (bShapeFilled || !insideEllipse(cx, cy, width));
					}
					if (bInside) {
						mark(px, py);
					}
				}
			}
		}
		ApplyStroke(previous.Union(StrokeDirty));
	}

	void PaintEditor::ClearMask(const PixelRect& rect)
	{
		const PixelRect clipped = rect.Intersect({ 0, 0, Doc.Width, Doc.Height });
		for (int y = clipped.Y; y < clipped.Bottom(); ++y) {
			std::fill_n(&StrokeMask[static_cast<size_t>(y) * Doc.Width + clipped.X], clipped.Width, 0.0f);
		}
	}

	void PaintEditor::ApplyStroke(const PixelRect& rect)
	{
		const PixelRect clipped = rect.Intersect({ 0, 0, Doc.Width, Doc.Height });
		Image& layer = *Doc.Layers[CurrentLayer].Pixels;
		const Image& base = *StrokeBase;
		const bool bErase = CurrentTool == Tool::Eraser;
		const Pixel color = bStrokeSecondary ? Secondary : Primary;
		const float opacity = CurrentTool == Tool::Brush ? BrushOpacity : bErase ? EraserOpacity : 1.0f;
		for (int y = clipped.Y; y < clipped.Bottom(); ++y) {
			for (int x = clipped.X; x < clipped.Right(); ++x) {
				const float mask = StrokeMask[static_cast<size_t>(y) * Doc.Width + x];
				const Pixel& before = base.At(x, y);
				if (mask <= 0.0f) {
					layer.At(x, y) = before;
				} else if (bErase) {
					Pixel erased = before;
					erased.A = static_cast<uint8_t>(std::clamp(before.A * (1.0f - mask * opacity) + 0.5f, 0.0f, 255.0f));
					layer.At(x, y) = erased;
				} else {
					layer.At(x, y) = BlendOver(before, color, mask * opacity);
				}
			}
		}
		MarkDirty(clipped);
	}

	// Marks the pixels to fill in the stroke mask; ApplyStroke paints them with the stroke's colour.
	void PaintEditor::FloodFill(int x, int y)
	{
		const PixelRect area = GetPaintArea();
		const Image& sample = bSampleAllLayers ? CompositeImage : *StrokeBase;
		std::vector<uint8_t> visited(static_cast<size_t>(Doc.Width) * Doc.Height, 0);
		const auto fillFrom = [&](int startX, int startY) {
			if (!area.Contains(startX, startY)) {
				return;
			}
			const Pixel target = sample.At(startX, startY);
			if (!bFillContiguous) {
				for (int py = area.Y; py < area.Bottom(); ++py) {
					for (int px = area.X; px < area.Right(); ++px) {
						if (IsSimilar(sample.At(px, py), target, FillTolerance)) {
							StrokeMask[static_cast<size_t>(py) * Doc.Width + px] = 1.0f;
						}
					}
				}
				StrokeDirty = StrokeDirty.Union(area);
				return;
			}
			std::vector<std::pair<int, int>> stack{ { startX, startY } };
			while (!stack.empty()) {
				const auto [px, py] = stack.back();
				stack.pop_back();
				if (!area.Contains(px, py)) {
					continue;
				}
				uint8_t& seen = visited[static_cast<size_t>(py) * Doc.Width + px];
				if (seen || !IsSimilar(sample.At(px, py), target, FillTolerance)) {
					continue;
				}
				seen = 1;
				StrokeMask[static_cast<size_t>(py) * Doc.Width + px] = 1.0f;
				StrokeDirty = StrokeDirty.Union({ px, py, 1, 1 });
				stack.push_back({ px + 1, py });
				stack.push_back({ px - 1, py });
				stack.push_back({ px, py + 1 });
				stack.push_back({ px, py - 1 });
			}
		};
		fillFrom(x, y);
		if (bMirrorX) {
			fillFrom(Doc.Width - 1 - x, y);
		}
		if (bMirrorY) {
			fillFrom(x, Doc.Height - 1 - y);
		}
		if (bMirrorX && bMirrorY) {
			fillFrom(Doc.Width - 1 - x, Doc.Height - 1 - y);
		}
		ApplyStroke(StrokeDirty);
	}

	void PaintEditor::PickColor(int x, int y, bool bSecondary)
	{
		if (x < 0 || y < 0 || x >= Doc.Width || y >= Doc.Height) {
			return;
		}
		const Pixel picked = bSampleAllLayers ? CompositeImage.At(x, y) : Doc.Layers[CurrentLayer].Pixels->At(x, y);
		(bSecondary ? Secondary : Primary) = picked;
	}

	void PaintEditor::UpdateMove(int deltaX, int deltaY)
	{
		const PixelRect canvas{ 0, 0, Doc.Width, Doc.Height };
		const PixelRect destination{ MoveSource.X + deltaX, MoveSource.Y + deltaY, MoveSource.Width, MoveSource.Height };
		const PixelRect area = MoveSource.Union(MoveLastDest).Union(destination).Intersect(canvas);
		Image& layer = *Doc.Layers[CurrentLayer].Pixels;
		const Image& base = *StrokeBase;
		for (int y = area.Y; y < area.Bottom(); ++y) {
			for (int x = area.X; x < area.Right(); ++x) {
				layer.At(x, y) = MoveSource.Contains(x, y) ? Pixel{} : base.At(x, y);
			}
		}
		const PixelRect visibleDestination = destination.Intersect(canvas);
		for (int y = visibleDestination.Y; y < visibleDestination.Bottom(); ++y) {
			for (int x = visibleDestination.X; x < visibleDestination.Right(); ++x) {
				layer.At(x, y) = BlendOver(layer.At(x, y), base.At(x - deltaX, y - deltaY), 1.0f);
			}
		}
		MoveLastDest = destination;
		if (!Selection.IsEmpty()) {
			Selection = destination;
		}
		MarkDirty(area);
	}

	// -----------------------
	// Edit / layer / canvas operations
	// -----------------------

	void PaintEditor::CopySelection(bool bCut)
	{
		const PixelRect area = GetPaintArea();
		Clipboard = Doc.Layers[CurrentLayer].Pixels->Crop(area);
		Message = "Copied " + std::to_string(area.Width) + " x " + std::to_string(area.Height) + " pixels.";
		if (bCut) {
			ClearSelection();
		}
	}

	void PaintEditor::Paste()
	{
		if (Clipboard.Width <= 0) {
			return;
		}
		const int x = Selection.IsEmpty() ? 0 : Selection.X;
		const int y = Selection.IsEmpty() ? 0 : Selection.Y;
		Image image(Doc.Width, Doc.Height);
		image.Paste(Clipboard, x, y, false);
		Selection = PixelRect{ x, y, Clipboard.Width, Clipboard.Height }.Intersect({ 0, 0, Doc.Width, Doc.Height });
		AddLayer("Pasted", &image);
		CurrentTool = Tool::Move;
	}

	void PaintEditor::ClearSelection()
	{
		if (!CanPaintOnLayer()) {
			return;
		}
		Doc.EditPixels(CurrentLayer).Fill(GetPaintArea(), Pixel{});
		MarkDirty(GetPaintArea());
		RecordUndo("Clear");
	}

	void PaintEditor::AddLayer(const std::string& name, const Image* image)
	{
		Layer layer{ name, std::make_shared<Image>(Doc.Width, Doc.Height) };
		if (image) {
			layer.Pixels->Paste(*image, 0, 0, false);
		}
		Doc.Layers.insert(Doc.Layers.begin() + CurrentLayer + 1, std::move(layer));
		++CurrentLayer;
		MarkAllDirty();
		RecordUndo(image ? "Import Layer" : "New Layer");
	}

	void PaintEditor::DeleteLayer()
	{
		if (Doc.Layers.size() <= 1) {
			return;
		}
		Doc.Layers.erase(Doc.Layers.begin() + CurrentLayer);
		CurrentLayer = std::min(CurrentLayer, static_cast<int>(Doc.Layers.size()) - 1);
		MarkAllDirty();
		RecordUndo("Delete Layer");
	}

	void PaintEditor::DuplicateLayer()
	{
		Layer copy = Doc.Layers[CurrentLayer]; // shares the pixels until one of them is painted on
		copy.Name += " copy";
		Doc.Layers.insert(Doc.Layers.begin() + CurrentLayer + 1, std::move(copy));
		++CurrentLayer;
		MarkAllDirty();
		RecordUndo("Duplicate Layer");
	}

	void PaintEditor::MoveLayer(int fromIndex, int toIndex)
	{
		const int count = static_cast<int>(Doc.Layers.size());
		if (fromIndex < 0 || toIndex < 0 || fromIndex >= count || toIndex >= count || fromIndex == toIndex) {
			return;
		}
		Layer layer = std::move(Doc.Layers[fromIndex]);
		Doc.Layers.erase(Doc.Layers.begin() + fromIndex);
		Doc.Layers.insert(Doc.Layers.begin() + toIndex, std::move(layer));
		CurrentLayer = toIndex;
		MarkAllDirty();
		RecordUndo("Move Layer");
	}

	void PaintEditor::MergeDown()
	{
		if (CurrentLayer <= 0) {
			return;
		}
		Doc.MergeDown(static_cast<size_t>(CurrentLayer));
		--CurrentLayer;
		MarkAllDirty();
		RecordUndo("Merge Down");
	}

	void PaintEditor::Flatten()
	{
		Layer flat{ "Flattened", std::make_shared<Image>(Doc.Flatten()) };
		Doc.Layers.clear();
		Doc.Layers.push_back(std::move(flat));
		CurrentLayer = 0;
		MarkAllDirty();
		RecordUndo("Flatten");
	}

	void PaintEditor::FlipLayer(bool bHorizontal)
	{
		if (!CanPaintOnLayer()) {
			return;
		}
		Image& image = Doc.EditPixels(CurrentLayer);
		bHorizontal ? image.FlipHorizontal() : image.FlipVertical();
		MarkAllDirty();
		RecordUndo(bHorizontal ? "Flip Layer Horizontally" : "Flip Layer Vertically");
	}

	void PaintEditor::FlipCanvas(bool bHorizontal)
	{
		for (size_t index = 0; index < Doc.Layers.size(); ++index) {
			Image& image = Doc.EditPixels(index);
			bHorizontal ? image.FlipHorizontal() : image.FlipVertical();
		}
		Selection = {};
		MarkAllDirty();
		RecordUndo(bHorizontal ? "Flip Canvas Horizontally" : "Flip Canvas Vertically");
	}

	void PaintEditor::RotateCanvas(bool bClockwise)
	{
		for (Layer& layer : Doc.Layers) {
			layer.Pixels = std::make_shared<Image>(bClockwise ? layer.Pixels->RotatedClockwise() : layer.Pixels->RotatedCounterClockwise());
		}
		std::swap(Doc.Width, Doc.Height);
		Selection = {};
		MarkAllDirty();
		bFitRequested = true;
		RecordUndo("Rotate Canvas");
	}

	void PaintEditor::ResizeCanvas(int width, int height, bool bScaleImage, bool bAnchorAtCentre)
	{
		if (width == Doc.Width && height == Doc.Height) {
			return;
		}
		const int offsetX = bAnchorAtCentre ? (width - Doc.Width) / 2 : 0;
		const int offsetY = bAnchorAtCentre ? (height - Doc.Height) / 2 : 0;
		for (Layer& layer : Doc.Layers) {
			if (bScaleImage) {
				layer.Pixels = std::make_shared<Image>(layer.Pixels->Resized(width, height));
			} else {
				auto resized = std::make_shared<Image>(width, height);
				resized->Paste(*layer.Pixels, offsetX, offsetY, false);
				layer.Pixels = std::move(resized);
			}
		}
		Doc.Width = width;
		Doc.Height = height;
		Selection = {};
		MarkAllDirty();
		bFitRequested = true;
		RecordUndo(bScaleImage ? "Image Size" : "Canvas Size");
	}

	void PaintEditor::ImportLayerFromFile(const std::filesystem::path& file)
	{
		Image image;
		std::string error;
		if (!LoadImageFile(file, image, error)) {
			Message = error;
			return;
		}
		if (image.Width > Doc.Width || image.Height > Doc.Height) {
			Message = "The image is bigger than the canvas: it was cropped (Canvas > Canvas Size to make room).";
		}
		AddLayer(PathToUtf8(file.stem()), &image);
	}

	void PaintEditor::ImportLayerFromTexture(Uid textureId)
	{
		if (const AssetInfo* texture = Context.Assets.Find(textureId)) {
			ImportLayerFromFile(Context.Assets.GetSourcePath(*texture));
		}
	}
}
