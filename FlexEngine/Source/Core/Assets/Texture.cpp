#include "Core/Assets/Texture.h"
#include "Core/FileSystem.h"
#include "Core/Graphics/Renderer.h"
#include "Core/Serialization/Archive.h"
#include <SDL3/SDL.h>

namespace Flex
{
	namespace
	{
		SDL_ScaleMode ToSDLScaleMode(TextureFilter filter)
		{
			switch (filter)
			{
			case TextureFilter::Nearest: return SDL_SCALEMODE_NEAREST;
			case TextureFilter::Linear: return SDL_SCALEMODE_LINEAR;
			case TextureFilter::PixelArt:
			default: return SDL_SCALEMODE_PIXELART;
			}
		}
	}

	const char* ToString(TextureFilter filter)
	{
		switch (filter)
		{
		case TextureFilter::Nearest: return "Nearest";
		case TextureFilter::Linear: return "Linear";
		case TextureFilter::PixelArt:
		default: return "PixelArt";
		}
	}

	bool TryParseTextureFilter(const std::string& text, TextureFilter& outFilter)
	{
		for (TextureFilter filter : { TextureFilter::PixelArt, TextureFilter::Nearest, TextureFilter::Linear }) {
			if (text == ToString(filter)) {
				outFilter = filter;
				return true;
			}
		}
		return false;
	}

	void TextureImportSettings::Serialize(Archive& archive)
	{
		archive.SerializeVersion(Version);

		// Stored as readable text ("PixelArt") instead of a number, so .meta files stay meaningful.
		std::string filterText = ToString(Filter);
		archive.Field("Filter", filterText);
		if (archive.IsLoading() && !TryParseTextureFilter(filterText, Filter)) {
			SDL_Log("Unknown texture filter '%s', using %s.", filterText.c_str(), ToString(Filter));
		}
	}

	Texture::~Texture()
	{
		if (!Handle) {
			return;
		}

		// If the renderer is already gone, SDL has freed this texture with it.
		if (Renderer::IsInitialised()) {
			SDL_DestroyTexture(Handle);
		}
		Renderer::OnTextureDestroyed();
	}

	std::shared_ptr<Texture> Texture::LoadFromFile(const std::filesystem::path& sourceFile, const TextureImportSettings& settings, std::string& outError)
	{
		if (!Renderer::IsInitialised()) {
			outError = "Renderer is not initialised.";
			return nullptr;
		}

		// SDL 3.4 reads PNG and BMP itself (no SDL_image needed).
		SDL_Surface* surface = SDL_LoadSurface(PathToUtf8(sourceFile).c_str());
		if (!surface) {
			outError = "Could not load " + PathToUtf8(sourceFile) + ": " + SDL_GetError();
			return nullptr;
		}

		SDL_Texture* handle = SDL_CreateTextureFromSurface(Renderer::GetSDLRenderer(), surface);
		const int width = surface->w;
		const int height = surface->h;
		SDL_DestroySurface(surface);

		if (!handle) {
			outError = "Could not create GPU texture for " + PathToUtf8(sourceFile) + ": " + SDL_GetError();
			return nullptr;
		}

		auto texture = std::shared_ptr<Texture>(new Texture());
		texture->Handle = handle;
		texture->Width = width;
		texture->Height = height;
		Renderer::OnTextureCreated();

		texture->SetFilter(settings.Filter);
		return texture;
	}

	std::shared_ptr<Texture> Texture::CreateFromPixels(int width, int height, const uint8_t* rgbaPixels, TextureFilter filter, std::string& outError)
	{
		if (!Renderer::IsInitialised()) {
			outError = "Renderer is not initialised.";
			return nullptr;
		}

		SDL_Texture* handle = SDL_CreateTexture(Renderer::GetSDLRenderer(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
		if (!handle) {
			outError = std::string("Could not create GPU texture: ") + SDL_GetError();
			return nullptr;
		}
		if (!SDL_UpdateTexture(handle, nullptr, rgbaPixels, width * 4)) {
			outError = std::string("Could not upload texture pixels: ") + SDL_GetError();
			SDL_DestroyTexture(handle);
			return nullptr;
		}
		SDL_SetTextureBlendMode(handle, SDL_BLENDMODE_BLEND);

		auto texture = std::shared_ptr<Texture>(new Texture());
		texture->Handle = handle;
		texture->Width = width;
		texture->Height = height;
		Renderer::OnTextureCreated();

		texture->SetFilter(filter);
		return texture;
	}

	bool Texture::UpdatePixels(int width, int height, const uint8_t* rgbaPixels, std::string& outError)
	{
		if (!Renderer::IsInitialised()) {
			outError = "Renderer is not initialised.";
			return false;
		}
		if (width <= 0 || height <= 0 || !rgbaPixels) {
			outError = "Invalid texture size.";
			return false;
		}

		// Same size and format: upload into the existing GPU texture.
		if (Handle && width == Width && height == Height && Handle->format == SDL_PIXELFORMAT_RGBA32) {
			if (!SDL_UpdateTexture(Handle, nullptr, rgbaPixels, width * 4)) {
				outError = std::string("Could not upload texture pixels: ") + SDL_GetError();
				return false;
			}
			return true;
		}

		// Another size or pixel format: a new GPU texture. The old one may still be used by this
		// frame's draw commands (e.g. an editor thumbnail), so the renderer frees it after the frame.
		SDL_Texture* handle = SDL_CreateTexture(Renderer::GetSDLRenderer(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
		if (!handle) {
			outError = std::string("Could not create GPU texture: ") + SDL_GetError();
			return false;
		}
		if (!SDL_UpdateTexture(handle, nullptr, rgbaPixels, width * 4)) {
			outError = std::string("Could not upload texture pixels: ") + SDL_GetError();
			SDL_DestroyTexture(handle);
			return false;
		}
		SDL_SetTextureBlendMode(handle, SDL_BLENDMODE_BLEND);

		if (Handle) {
			Renderer::DestroyAfterFrame(Handle);
		} else {
			Renderer::OnTextureCreated();
		}
		Handle = handle;
		Width = width;
		Height = height;
		SetFilter(Filter);
		return true;
	}

	size_t Texture::GetMemorySize() const
	{
		return static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4;
	}

	void Texture::SetFilter(TextureFilter filter)
	{
		Filter = filter;
		if (Handle) {
			SDL_SetTextureScaleMode(Handle, ToSDLScaleMode(filter));
		}
	}
}
