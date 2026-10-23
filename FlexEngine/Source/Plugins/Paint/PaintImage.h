#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace Flex::Paint
{
	// One pixel: RGBA8 with straight (not premultiplied) alpha, the byte order of SDL_PIXELFORMAT_RGBA32.
	struct Pixel
	{
		uint8_t R = 0;
		uint8_t G = 0;
		uint8_t B = 0;
		uint8_t A = 0;

		bool operator==(const Pixel&) const = default;
	};

	// Integer rectangle in pixels (top-left + size). Empty when Width or Height <= 0.
	struct PixelRect
	{
		int X = 0;
		int Y = 0;
		int Width = 0;
		int Height = 0;

		bool IsEmpty() const { return Width <= 0 || Height <= 0; }
		int Right() const { return X + Width; }
		int Bottom() const { return Y + Height; }
		bool Contains(int x, int y) const { return x >= X && y >= Y && x < Right() && y < Bottom(); }
		bool operator==(const PixelRect&) const = default;

		static PixelRect FromCorners(int x0, int y0, int x1, int y1) // exclusive x1/y1
		{
			return { std::min(x0, x1), std::min(y0, y1), std::abs(x1 - x0), std::abs(y1 - y0) };
		}

		PixelRect Union(const PixelRect& other) const
		{
			if (IsEmpty()) {
				return other;
			}
			if (other.IsEmpty()) {
				return *this;
			}
			const int x = std::min(X, other.X);
			const int y = std::min(Y, other.Y);
			return { x, y, std::max(Right(), other.Right()) - x, std::max(Bottom(), other.Bottom()) - y };
		}

		PixelRect Intersect(const PixelRect& other) const
		{
			const int x = std::max(X, other.X);
			const int y = std::max(Y, other.Y);
			const int right = std::min(Right(), other.Right());
			const int bottom = std::min(Bottom(), other.Bottom());
			return { x, y, std::max(0, right - x), std::max(0, bottom - y) };
		}
	};

	// A width * height grid of pixels, row by row, top row first.
	struct Image
	{
		int Width = 0;
		int Height = 0;
		std::vector<Pixel> Pixels;

		Image() = default;
		Image(int width, int height, Pixel fill = {}) : Width{ width }, Height{ height }, Pixels(static_cast<size_t>(width) * height, fill) {}

		PixelRect GetRect() const { return { 0, 0, Width, Height }; }
		Pixel& At(int x, int y) { return Pixels[static_cast<size_t>(y) * Width + x]; }
		const Pixel& At(int x, int y) const { return Pixels[static_cast<size_t>(y) * Width + x]; }
		const uint8_t* Data() const { return reinterpret_cast<const uint8_t*>(Pixels.data()); }

		// The part inside rect as a new image (rect is clipped to this image).
		Image Crop(const PixelRect& rect) const;
		// Draws source with its top-left at (x, y): alpha-blended over, or replacing the pixels.
		void Paste(const Image& source, int x, int y, bool bBlend);
		void Fill(const PixelRect& rect, Pixel pixel);
		void FlipHorizontal();
		void FlipVertical();
		Image RotatedClockwise() const;
		Image RotatedCounterClockwise() const;
		Image Resized(int width, int height) const; // nearest neighbour
	};

	// Straight-alpha "source over destination", the source's alpha first scaled by amount (0-1).
	Pixel BlendOver(Pixel destination, Pixel source, float amount);

	// PNG in memory (SDL 3.4's own encoder/decoder).
	bool EncodePng(const Image& image, std::vector<uint8_t>& outPng, std::string& outError);
	bool DecodePng(const uint8_t* data, size_t size, Image& outImage, std::string& outError);

	// Any image file SDL can read (.png, .bmp), converted to RGBA.
	bool LoadImageFile(const std::filesystem::path& file, Image& outImage, std::string& outError);
	bool SavePngFile(const Image& image, const std::filesystem::path& file, std::string& outError);

	std::string EncodeBase64(const std::vector<uint8_t>& data);
	bool DecodeBase64(const std::string& text, std::vector<uint8_t>& outData);
}
