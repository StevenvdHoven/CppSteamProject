#include "Plugins/Paint/PaintImage.h"
#include "Core/FileSystem.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <cstring>

namespace Flex::Paint
{
	namespace
	{
		constexpr char BASE64CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		// Copies an SDL surface of any format into an RGBA image.
		bool SurfaceToImage(SDL_Surface* surface, Image& outImage, std::string& outError)
		{
			SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
			if (!rgba) {
				outError = std::string("Could not convert the image: ") + SDL_GetError();
				return false;
			}
			outImage = Image(rgba->w, rgba->h);
			for (int y = 0; y < rgba->h; ++y) {
				std::memcpy(&outImage.At(0, y), static_cast<const uint8_t*>(rgba->pixels) + static_cast<size_t>(y) * rgba->pitch, static_cast<size_t>(rgba->w) * 4);
			}
			SDL_DestroySurface(rgba);
			return true;
		}

		// A surface that uses the image's pixels (no copy); destroy it before the image changes.
		SDL_Surface* SurfaceFromImage(const Image& image, std::string& outError)
		{
			SDL_Surface* surface = SDL_CreateSurfaceFrom(image.Width, image.Height, SDL_PIXELFORMAT_RGBA32,
				const_cast<uint8_t*>(image.Data()), image.Width * 4);
			if (!surface) {
				outError = std::string("Could not create an image surface: ") + SDL_GetError();
			}
			return surface;
		}
	}

	Image Image::Crop(const PixelRect& rect) const
	{
		const PixelRect clipped = rect.Intersect(GetRect());
		Image result(clipped.Width, clipped.Height);
		for (int y = 0; y < clipped.Height; ++y) {
			for (int x = 0; x < clipped.Width; ++x) {
				result.At(x, y) = At(clipped.X + x, clipped.Y + y);
			}
		}
		return result;
	}

	void Image::Paste(const Image& source, int x, int y, bool bBlend)
	{
		const PixelRect target = PixelRect{ x, y, source.Width, source.Height }.Intersect(GetRect());
		for (int row = target.Y; row < target.Bottom(); ++row) {
			for (int column = target.X; column < target.Right(); ++column) {
				const Pixel& pixel = source.At(column - x, row - y);
				At(column, row) = bBlend ? BlendOver(At(column, row), pixel, 1.0f) : pixel;
			}
		}
	}

	void Image::Fill(const PixelRect& rect, Pixel pixel)
	{
		const PixelRect clipped = rect.Intersect(GetRect());
		for (int y = clipped.Y; y < clipped.Bottom(); ++y) {
			std::fill_n(&At(clipped.X, y), clipped.Width, pixel);
		}
	}

	void Image::FlipHorizontal()
	{
		for (int y = 0; y < Height; ++y) {
			std::reverse(&At(0, y), &At(0, y) + Width);
		}
	}

	void Image::FlipVertical()
	{
		for (int y = 0; y < Height / 2; ++y) {
			std::swap_ranges(&At(0, y), &At(0, y) + Width, &At(0, Height - 1 - y));
		}
	}

	Image Image::RotatedClockwise() const
	{
		Image result(Height, Width);
		for (int y = 0; y < Height; ++y) {
			for (int x = 0; x < Width; ++x) {
				result.At(Height - 1 - y, x) = At(x, y);
			}
		}
		return result;
	}

	Image Image::RotatedCounterClockwise() const
	{
		Image result(Height, Width);
		for (int y = 0; y < Height; ++y) {
			for (int x = 0; x < Width; ++x) {
				result.At(y, Width - 1 - x) = At(x, y);
			}
		}
		return result;
	}

	Image Image::Resized(int width, int height) const
	{
		Image result(width, height);
		for (int y = 0; y < height; ++y) {
			const int sourceY = std::min(Height - 1, static_cast<int>((static_cast<int64_t>(y) * Height) / height));
			for (int x = 0; x < width; ++x) {
				const int sourceX = std::min(Width - 1, static_cast<int>((static_cast<int64_t>(x) * Width) / width));
				result.At(x, y) = At(sourceX, sourceY);
			}
		}
		return result;
	}

	Pixel BlendOver(Pixel destination, Pixel source, float amount)
	{
		const float sourceAlpha = source.A / 255.0f * amount;
		if (sourceAlpha <= 0.0f) {
			return destination;
		}
		const float destinationAlpha = destination.A / 255.0f;
		const float outAlpha = sourceAlpha + destinationAlpha * (1.0f - sourceAlpha);
		const auto channel = [&](uint8_t s, uint8_t d) {
			const float value = (s * sourceAlpha + d * destinationAlpha * (1.0f - sourceAlpha)) / outAlpha;
			return static_cast<uint8_t>(std::clamp(value + 0.5f, 0.0f, 255.0f));
		};
		return { channel(source.R, destination.R), channel(source.G, destination.G), channel(source.B, destination.B),
			static_cast<uint8_t>(std::clamp(outAlpha * 255.0f + 0.5f, 0.0f, 255.0f)) };
	}

	bool EncodePng(const Image& image, std::vector<uint8_t>& outPng, std::string& outError)
	{
		SDL_Surface* surface = SurfaceFromImage(image, outError);
		if (!surface) {
			return false;
		}
		SDL_IOStream* stream = SDL_IOFromDynamicMem();
		const bool bSaved = stream && SDL_SavePNG_IO(surface, stream, false);
		SDL_DestroySurface(surface);
		if (!bSaved) {
			outError = std::string("Could not encode PNG: ") + SDL_GetError();
			if (stream) {
				SDL_CloseIO(stream);
			}
			return false;
		}
		const Sint64 size = SDL_GetIOSize(stream);
		const auto* data = static_cast<const uint8_t*>(SDL_GetPointerProperty(SDL_GetIOProperties(stream), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, nullptr));
		outPng.assign(data, data + std::max<Sint64>(size, 0));
		SDL_CloseIO(stream);
		return true;
	}

	bool DecodePng(const uint8_t* data, size_t size, Image& outImage, std::string& outError)
	{
		SDL_IOStream* stream = SDL_IOFromConstMem(data, size);
		SDL_Surface* surface = stream ? SDL_LoadPNG_IO(stream, true) : nullptr;
		if (!surface) {
			outError = std::string("Could not decode PNG: ") + SDL_GetError();
			return false;
		}
		const bool bConverted = SurfaceToImage(surface, outImage, outError);
		SDL_DestroySurface(surface);
		return bConverted;
	}

	bool LoadImageFile(const std::filesystem::path& file, Image& outImage, std::string& outError)
	{
		SDL_Surface* surface = SDL_LoadSurface(PathToUtf8(file).c_str());
		if (!surface) {
			outError = "Could not load " + PathToUtf8(file) + ": " + SDL_GetError();
			return false;
		}
		const bool bConverted = SurfaceToImage(surface, outImage, outError);
		SDL_DestroySurface(surface);
		return bConverted;
	}

	bool SavePngFile(const Image& image, const std::filesystem::path& file, std::string& outError)
	{
		SDL_Surface* surface = SurfaceFromImage(image, outError);
		if (!surface) {
			return false;
		}
		const bool bSaved = SDL_SavePNG(surface, PathToUtf8(file).c_str());
		SDL_DestroySurface(surface);
		if (!bSaved) {
			outError = "Could not save " + PathToUtf8(file) + ": " + SDL_GetError();
		}
		return bSaved;
	}

	std::string EncodeBase64(const std::vector<uint8_t>& data)
	{
		std::string text;
		text.reserve((data.size() + 2) / 3 * 4);
		for (size_t index = 0; index < data.size(); index += 3) {
			const uint32_t a = data[index];
			const uint32_t b = index + 1 < data.size() ? data[index + 1] : 0;
			const uint32_t c = index + 2 < data.size() ? data[index + 2] : 0;
			const uint32_t triple = (a << 16) | (b << 8) | c;
			text += BASE64CHARS[(triple >> 18) & 63];
			text += BASE64CHARS[(triple >> 12) & 63];
			text += index + 1 < data.size() ? BASE64CHARS[(triple >> 6) & 63] : '=';
			text += index + 2 < data.size() ? BASE64CHARS[triple & 63] : '=';
		}
		return text;
	}

	bool DecodeBase64(const std::string& text, std::vector<uint8_t>& outData)
	{
		const auto value = [](char c) -> int {
			if (c >= 'A' && c <= 'Z') return c - 'A';
			if (c >= 'a' && c <= 'z') return c - 'a' + 26;
			if (c >= '0' && c <= '9') return c - '0' + 52;
			if (c == '+') return 62;
			if (c == '/') return 63;
			return -1;
		};
		outData.clear();
		outData.reserve(text.size() / 4 * 3);
		uint32_t bits = 0;
		int bitCount = 0;
		for (const char c : text) {
			if (c == '=') {
				break;
			}
			const int digit = value(c);
			if (digit < 0) {
				return false;
			}
			bits = (bits << 6) | static_cast<uint32_t>(digit);
			bitCount += 6;
			if (bitCount >= 8) {
				bitCount -= 8;
				outData.push_back(static_cast<uint8_t>((bits >> bitCount) & 0xFF));
			}
		}
		return true;
	}
}
