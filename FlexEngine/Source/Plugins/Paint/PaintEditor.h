#pragma once
#include "Plugins/Paint/Painting.h"
#include "Editor/EditorWindow.h"
#include "Editor/FileDialog.h"
#include "Core/Uid.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;
struct ImDrawList;

namespace Flex
{
	class Texture;
	struct EditorContext;
}

namespace Flex::Paint
{
	// A list of the project's textures with a search box; returns true (and outChosen) on a click.
	bool DrawTexturePicker(EditorContext& context, char* search, size_t searchSize, Uid& outChosen);

	// The Paint window for one .flexpaint asset: tools on the left, the canvas in the middle,
	// tool options, colours, layers and export settings on the right.
	// Edits are undoable (Ctrl+Z/Y while the window is focused, separate from scene undo);
	// Ctrl+S saves the painting, Ctrl+E exports it to its texture (.png).
	class PaintEditor final : public AssetEditor
	{
	public:
		PaintEditor(EditorContext& context, SDL_Window* mainWindow, Uid paintingId);
		~PaintEditor() override;

		void Draw() override;

		bool HasUnsavedChanges() const;

	private:
		enum class Tool : uint8_t
		{
			Pencil,
			Brush,
			Eraser,
			Fill,
			Picker,
			Line,
			Rectangle,
			Ellipse,
			Select,
			Move,
		};

		// One undo step: the whole document. Layers share their pixels with the
		// neighbouring steps until one of them is painted on (Painting::EditPixels).
		struct HistoryState
		{
			std::string Name;
			int Width = 0;
			int Height = 0;
			std::vector<Layer> Layers;
			int CurrentLayer = 0;
			PixelRect Selection;
			uint64_t Revision = 0;
		};

		void Load();
		bool Save();
		// Writes the flattened image to ExportPath and updates the loaded texture. bConfirmed skips the overwrite question.
		bool Export(bool bConfirmed = false);
		void UpdateTitle();

		// Undo
		void RecordUndo(const std::string& name);
		void ResetHistory();
		void RestoreState(const HistoryState& state);
		void Undo();
		void Redo();

		// Drawing the window
		void DrawMenuBar();
		void DrawToolbar();
		void DrawCanvas();
		void DrawSidebar();
		void DrawToolOptions();
		void DrawColors();
		void DrawLayers();
		void DrawExport();
		void DrawPopups();
		void HandleShortcuts();
		void UpdateDisplay();
		void FitToView(float width, float height);

		// Painting
		bool CanPaintOnLayer();
		bool IsShapeTool() const { return CurrentTool == Tool::Line || CurrentTool == Tool::Rectangle || CurrentTool == Tool::Ellipse; }
		void BeginStroke(float x, float y, bool bSecondary);
		void ContinueStroke(float x, float y);
		void EndStroke();
		void StampDab(float x, float y);                     // with mirroring
		void StampDabAt(float x, float y, int size, float hardness, bool bHard);
		void StampLine(float x0, float y0, float x1, float y1);
		void StampShape();
		void ClearMask(const PixelRect& rect);
		void ApplyStroke(const PixelRect& rect);              // layer = stroke base + mask, inside rect
		void FloodFill(int x, int y);
		void PickColor(int x, int y, bool bSecondary);
		void UpdateMove(int deltaX, int deltaY);
		PixelRect GetPaintArea() const;                       // selection or the whole canvas
		void MarkDirty(const PixelRect& rect);
		void MarkAllDirty();

		// Edit / layer / canvas operations (each one undo step)
		void CopySelection(bool bCut);
		void Paste();
		void ClearSelection();
		void AddLayer(const std::string& name, const Image* image);
		void DeleteLayer();
		void DuplicateLayer();
		void MoveLayer(int fromIndex, int toIndex);
		void MergeDown();
		void Flatten();
		void FlipLayer(bool bHorizontal);
		void FlipCanvas(bool bHorizontal);
		void RotateCanvas(bool bClockwise);
		void ResizeCanvas(int width, int height, bool bScaleImage, bool bAnchorCentre);
		void ImportLayerFromFile(const std::filesystem::path& file);
		void ImportLayerFromTexture(Uid textureId);

	private:
		EditorContext& Context;
		SDL_Window* MainWindow = nullptr;
		Uid PaintingId;

		Painting Doc;
		bool bIsLoaded = false;
		std::string Error;
		std::string Message;
		int CurrentLayer = 0;
		PixelRect Selection; // empty = everything

		// Tools
		Tool CurrentTool = Tool::Pencil;
		Pixel Primary{ 0, 0, 0, 255 };
		Pixel Secondary{ 255, 255, 255, 255 };
		int PencilSize = 1;
		int BrushSize = 8;
		float BrushHardness = 0.5f;
		float BrushOpacity = 1.0f;
		float BrushSpacing = 0.15f; // of the size
		int EraserSize = 4;
		float EraserHardness = 1.0f;
		float EraserOpacity = 1.0f;
		int FillTolerance = 0;
		bool bFillContiguous = true;
		bool bSampleAllLayers = false;
		int ShapeWidth = 1;
		bool bShapeFilled = false;
		bool bMirrorX = false;
		bool bMirrorY = false;
		bool bShowGrid = true;

		// View
		float Zoom = 1.0f;
		float PanX = 0.0f;
		float PanY = 0.0f;
		bool bFitRequested = true;
		bool bHasMouse = false;
		int MouseX = 0;
		int MouseY = 0;

		// What the canvas shows: every visible layer composited (updated only where something changed).
		Image CompositeImage;
		PixelRect CompositeDirty;
		std::shared_ptr<Texture> DisplayTexture;
		std::shared_ptr<Texture> CheckerTexture;

		// The stroke in progress (also shapes, fill, move and selecting)
		bool bStroking = false;
		bool bStrokeSecondary = false;
		std::shared_ptr<const Image> StrokeBase; // the layer as it was when the stroke started
		std::vector<float> StrokeMask;           // coverage 0-1 per pixel
		PixelRect StrokeDirty;  // everything the stroke touched
		PixelRect PendingDirty; // touched since the layer was last updated
		float StrokeStartX = 0.0f;
		float StrokeStartY = 0.0f;
		float StrokeLastX = 0.0f;
		float StrokeLastY = 0.0f;
		float StrokeEndX = 0.0f;
		float StrokeEndY = 0.0f;
		PixelRect MoveSource;
		PixelRect MoveLastDest;
		bool bSelecting = false;

		// Undo
		std::vector<HistoryState> History;
		size_t HistoryIndex = 0;
		uint64_t RevisionCounter = 0;
		uint64_t SavedRevision = 0;
		bool bSettingsChanged = false; // export settings / palette: saved with the file, not undoable

		// Dialogs
		bool bRequestUnsavedPopup = false;
		bool bRequestOverwritePopup = false;
		bool bRequestSizePopup = false;
		bool bSizePopupScales = false;
		int NewWidth = 0;
		int NewHeight = 0;
		bool bAnchorCentre = true;
		bool bRequestTexturePopup = false;
		char TextureSearch[64]{};
		char ExportPathBuffer[260]{};
		FileDialog ImportDialog;
		std::string LastTitle;
	};
}
