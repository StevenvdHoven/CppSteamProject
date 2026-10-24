#pragma once
#include "Core/Assets/Asset.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

struct SDL_Texture;
struct SDL_Surface;

namespace Flex
{
	class Archive;

	enum class TextureFilter : uint8_t
	{
		PixelArt, // sharp pixels that still scale smoothly at non-integer zoom (SDL 3.4)
		Nearest,  // classic sharp pixels
		Linear    // smooth, for high-resolution art
	};

	FLEX_API const char* ToString(TextureFilter filter);
	FLEX_API bool TryParseTextureFilter(const std::string& text, TextureFilter& outFilter);

	// Stored in the [Import] section of the texture's .meta.
	struct FLEX_API TextureImportSettings
	{
		static constexpr uint32_t Version = 1;

		TextureFilter Filter = TextureFilter::PixelArt;

		void Serialize(Archive& archive);
	};

	// One image on the GPU. Loaded once and shared by everything that uses it
	// (sprites cut from it reference this same object).
	class FLEX_API Texture final : public Asset
	{
	public:
		~Texture() override;

		// Loads a .png or .bmp from disk and uploads it to the GPU. Needs Renderer::Init first.
		static std::shared_ptr<Texture> LoadFromFile(const std::filesystem::path& sourceFile, const TextureImportSettings& settings, std::string& outError);

		// Creates a texture from width * height RGBA pixels (4 bytes each, row by row, top row first),
		// e.g. for generated images. Needs Renderer::Init first.
		static std::shared_ptr<Texture> CreateFromPixels(int width, int height, const uint8_t* rgbaPixels, TextureFilter filter, std::string& outError);

		// Replaces the image of this (already loaded) texture with width * height RGBA pixels, in place:
		// every sprite and renderer holding it shows the new image at once. The size may change
		// (then sprites keep their pixel rects). Keeps the filter. Used by the editor after an asset is rewritten.
		bool UpdatePixels(int width, int height, const uint8_t* rgbaPixels, std::string& outError);

		static constexpr const char* TypeName = "Texture";
		const char* GetTypeName() const override { return TypeName; }
		size_t GetMemorySize() const override;

		int GetWidth() const { return Width; }
		int GetHeight() const { return Height; }

		TextureFilter GetFilter() const { return Filter; }
		// Applies immediately to everything drawing this texture.
		void SetFilter(TextureFilter filter);

		SDL_Texture* GetSDLTexture() const { return Handle; }

	private:
		Texture() = default;

	private:
		SDL_Texture* Handle = nullptr;
		int Width = 0;
		int Height = 0;
		TextureFilter Filter = TextureFilter::PixelArt;
	};
}
