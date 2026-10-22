#pragma once
#include "Core/Assets/Sprite.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Flex
{
	struct GridSliceSettings
	{
		enum class SliceMode
		{
			CellSize,  // fixed cell width/height in pixels
			CellCount  // fixed number of columns/rows; cell size follows from the texture size
		};

		SliceMode Mode = SliceMode::CellSize;
		int32_t CellWidth = 32;
		int32_t CellHeight = 32;
		int32_t Columns = 4;
		int32_t Rows = 1;
		int32_t OffsetX = 0;  // pixels skipped at the left/top before the first cell
		int32_t OffsetY = 0;
		int32_t PaddingX = 0; // pixels between cells
		int32_t PaddingY = 0;
		bool bSkipEmptyCells = true;
	};

	// Pure sprite-sheet logic (no UI), used by the Texture Editor.
	namespace SpriteSlicer
	{
		// Safety limit, so a 1x1 cell size on a big texture can't create millions of sprites.
		constexpr size_t MaxSprites = 4096;

		// Row by row, left to right. Only whole cells that fit inside the texture.
		std::vector<SpriteRect> SliceGrid(const GridSliceSettings& settings, int32_t textureWidth, int32_t textureHeight);

		// Removes cells whose pixels are all fully transparent. Reads the source image from disk.
		bool RemoveEmptyCells(const std::filesystem::path& sourceFile, std::vector<SpriteRect>& rects, std::string& outError);

		// rgba: 4 bytes per pixel (R, G, B, A), rows `pitch` bytes apart.
		bool HasVisiblePixels(const uint8_t* rgba, int32_t pitch, const SpriteRect& rect);

		// Turns rects into sprites named <namePrefix><index>. A sprite whose name already
		// exists keeps its Id, so anything referencing it keeps working after a re-slice.
		std::vector<SpriteDefinition> BuildSprites(const std::vector<SpriteRect>& rects, const std::string& namePrefix, const std::vector<SpriteDefinition>& existing, float pivotX, float pivotY);

		// baseName if it's free, otherwise baseName_1, baseName_2, ...
		std::string MakeUniqueName(const std::string& baseName, const std::vector<SpriteDefinition>& sprites);
	}
}
