#include "Editor/SpriteSlicer.h"
#include "Core/FileSystem.h"
#include <SDL3/SDL.h>
#include <algorithm>

namespace Flex::SpriteSlicer
{
	std::vector<SpriteRect> SliceGrid(const GridSliceSettings& settings, int32_t textureWidth, int32_t textureHeight)
	{
		std::vector<SpriteRect> rects;

		const int32_t offsetX = std::max(settings.OffsetX, 0);
		const int32_t offsetY = std::max(settings.OffsetY, 0);
		const int32_t paddingX = std::max(settings.PaddingX, 0);
		const int32_t paddingY = std::max(settings.PaddingY, 0);

		int32_t cellWidth = settings.CellWidth;
		int32_t cellHeight = settings.CellHeight;
		if (settings.Mode == GridSliceSettings::SliceMode::CellCount)
		{
			const int32_t columns = std::max(settings.Columns, 1);
			const int32_t rows = std::max(settings.Rows, 1);
			cellWidth = (textureWidth - offsetX - paddingX * (columns - 1)) / columns;
			cellHeight = (textureHeight - offsetY - paddingY * (rows - 1)) / rows;
		}
		if (cellWidth < 1 || cellHeight < 1) {
			return rects;
		}

		for (int32_t y = offsetY; y + cellHeight <= textureHeight; y += cellHeight + paddingY)
		{
			for (int32_t x = offsetX; x + cellWidth <= textureWidth; x += cellWidth + paddingX)
			{
				if (rects.size() >= MaxSprites) {
					return rects;
				}
				rects.push_back(SpriteRect{ x, y, cellWidth, cellHeight });
			}
		}
		return rects;
	}

	bool HasVisiblePixels(const uint8_t* rgba, int32_t pitch, const SpriteRect& rect)
	{
		for (int32_t y = rect.Y; y < rect.Y + rect.Height; ++y)
		{
			const uint8_t* row = rgba + static_cast<size_t>(y) * pitch;
			for (int32_t x = rect.X; x < rect.X + rect.Width; ++x) {
				if (row[static_cast<size_t>(x) * 4 + 3] != 0) {
					return true;
				}
			}
		}
		return false;
	}

	bool RemoveEmptyCells(const std::filesystem::path& sourceFile, std::vector<SpriteRect>& rects, std::string& outError)
	{
		SDL_Surface* loaded = SDL_LoadSurface(PathToUtf8(sourceFile).c_str());
		if (!loaded) {
			outError = "Could not read " + PathToUtf8(sourceFile) + ": " + SDL_GetError();
			return false;
		}

		// RGBA32 = bytes R, G, B, A in memory on every platform.
		SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
		SDL_DestroySurface(loaded);
		if (!rgba) {
			outError = std::string("Could not convert the image: ") + SDL_GetError();
			return false;
		}

		const uint8_t* pixels = static_cast<const uint8_t*>(rgba->pixels);
		std::erase_if(rects, [&](const SpriteRect& rect) {
			const SpriteRect inside = rect.ClampedTo(rgba->w, rgba->h);
			return !HasVisiblePixels(pixels, rgba->pitch, inside);
		});

		SDL_DestroySurface(rgba);
		return true;
	}

	std::vector<SpriteDefinition> BuildSprites(const std::vector<SpriteRect>& rects, const std::string& namePrefix, const std::vector<SpriteDefinition>& existing, float pivotX, float pivotY)
	{
		std::vector<SpriteDefinition> sprites;
		sprites.reserve(rects.size());

		for (size_t index{ 0 }; index < rects.size(); ++index)
		{
			SpriteDefinition sprite;
			sprite.Name = namePrefix + std::to_string(index);
			sprite.Rect = rects[index];
			sprite.PivotX = pivotX;
			sprite.PivotY = pivotY;

			const auto previous = std::find_if(existing.begin(), existing.end(), [&](const SpriteDefinition& other) { return other.Name == sprite.Name; });
			sprite.Id = previous != existing.end() ? previous->Id : Uid::Generate();

			sprites.push_back(std::move(sprite));
		}
		return sprites;
	}

	std::string MakeUniqueName(const std::string& baseName, const std::vector<SpriteDefinition>& sprites)
	{
		auto isTaken = [&](const std::string& name) {
			return std::any_of(sprites.begin(), sprites.end(), [&](const SpriteDefinition& sprite) { return sprite.Name == name; });
		};

		if (!isTaken(baseName)) {
			return baseName;
		}
		for (int suffix = 1;; ++suffix) {
			const std::string candidate = baseName + "_" + std::to_string(suffix);
			if (!isTaken(candidate)) {
				return candidate;
			}
		}
	}
}
