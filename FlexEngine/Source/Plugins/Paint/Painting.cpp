#include "Plugins/Paint/Painting.h"
#include "Core/FileSystem.h"
#include "Core/Serialization/TextArchive.h"
#include "Core/Serialization/TextDocument.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace Flex::Paint
{
	namespace
	{
		constexpr const char* PAINTINGSECTION = "Painting";
		constexpr const char* LAYERSECTION = "Layer";

		// DawnBringer's 32-colour palette, a common default for pixel art.
		constexpr uint32_t DEFAULTPALETTE[] = {
			0x000000ff, 0x222034ff, 0x45283cff, 0x663931ff, 0x8f563bff, 0xdf7126ff, 0xd9a066ff, 0xeec39aff,
			0xfbf236ff, 0x99e550ff, 0x6abe30ff, 0x37946eff, 0x4b692fff, 0x524b24ff, 0x323c39ff, 0x3f3f74ff,
			0x306082ff, 0x5b6ee1ff, 0x639bffff, 0x5fcde4ff, 0xcbdbfcff, 0xffffffff, 0x9badb7ff, 0x847e87ff,
			0x696a6aff, 0x595652ff, 0x76428aff, 0xac3232ff, 0xd95763ff, 0xd77bbaff, 0x8f974aff, 0x8a6f30ff,
		};

		Pixel FromHex(uint32_t rgba)
		{
			return { static_cast<uint8_t>(rgba >> 24), static_cast<uint8_t>(rgba >> 16), static_cast<uint8_t>(rgba >> 8), static_cast<uint8_t>(rgba) };
		}

		std::string PaletteToString(const std::vector<Pixel>& palette)
		{
			std::string text;
			char buffer[12];
			for (const Pixel& pixel : palette) {
				std::snprintf(buffer, sizeof(buffer), "%02x%02x%02x%02x", pixel.R, pixel.G, pixel.B, pixel.A);
				text += text.empty() ? "" : " ";
				text += buffer;
			}
			return text;
		}

		std::vector<Pixel> PaletteFromString(const std::string& text)
		{
			std::vector<Pixel> palette;
			std::istringstream stream(text);
			std::string word;
			while (stream >> word) {
				if (word.size() == 8) {
					palette.push_back(FromHex(static_cast<uint32_t>(std::strtoul(word.c_str(), nullptr, 16))));
				}
			}
			return palette;
		}

		// Separable blend of a backdrop and a source colour (0-1).
		float BlendChannel(BlendMode mode, float backdrop, float source)
		{
			switch (mode)
			{
			case BlendMode::Multiply: return backdrop * source;
			case BlendMode::Add: return std::min(1.0f, backdrop + source);
			case BlendMode::Screen: return backdrop + source - backdrop * source;
			case BlendMode::Normal:
			default: return source;
			}
		}

		// Composites one layer pixel onto a premultiplied float backdrop (r, g, b, a).
		void CompositePixel(float* backdrop, const Pixel& source, float opacity, BlendMode mode)
		{
			const float sourceAlpha = source.A / 255.0f * opacity;
			if (sourceAlpha <= 0.0f) {
				return;
			}
			const float backdropAlpha = backdrop[3];
			const float sourceColor[3] = { source.R / 255.0f, source.G / 255.0f, source.B / 255.0f };
			for (int channel = 0; channel < 3; ++channel) {
				const float backdropColor = backdropAlpha > 0.0f ? backdrop[channel] / backdropAlpha : 0.0f;
				// Where the backdrop is transparent the source colour shows as it is (W3C compositing).
				const float mixed = (1.0f - backdropAlpha) * sourceColor[channel] + backdropAlpha * BlendChannel(mode, backdropColor, sourceColor[channel]);
				backdrop[channel] = sourceAlpha * mixed + backdrop[channel] * (1.0f - sourceAlpha);
			}
			backdrop[3] = sourceAlpha + backdropAlpha * (1.0f - sourceAlpha);
		}

		Pixel ToPixel(const float* premultiplied)
		{
			const float alpha = premultiplied[3];
			if (alpha <= 0.0f) {
				return {};
			}
			const auto channel = [alpha](float value) { return static_cast<uint8_t>(std::clamp(value / alpha * 255.0f + 0.5f, 0.0f, 255.0f)); };
			return { channel(premultiplied[0]), channel(premultiplied[1]), channel(premultiplied[2]), static_cast<uint8_t>(std::clamp(alpha * 255.0f + 0.5f, 0.0f, 255.0f)) };
		}
	}

	const char* ToString(BlendMode mode)
	{
		switch (mode)
		{
		case BlendMode::Multiply: return "Multiply";
		case BlendMode::Add: return "Add";
		case BlendMode::Screen: return "Screen";
		case BlendMode::Normal:
		default: return "Normal";
		}
	}

	bool BlendModeFromString(const std::string& text, BlendMode& outMode)
	{
		for (const BlendMode mode : AllBlendModes) {
			if (text == ToString(mode)) {
				outMode = mode;
				return true;
			}
		}
		return false;
	}

	Painting Painting::Create(int width, int height, Pixel background)
	{
		Painting painting;
		painting.Width = width;
		painting.Height = height;
		painting.Layers.push_back(Layer{ "Layer 1", std::make_shared<Image>(width, height, background) });
		painting.Palette = DefaultPalette();
		return painting;
	}

	std::vector<Pixel> Painting::DefaultPalette()
	{
		std::vector<Pixel> palette;
		for (const uint32_t rgba : DEFAULTPALETTE) {
			palette.push_back(FromHex(rgba));
		}
		return palette;
	}

	void Painting::SerializeHeader(Archive& archive)
	{
		archive.SerializeVersion(Version);
		archive.Field("Width", Width);
		archive.Field("Height", Height);
		archive.Field("ExportPath", ExportPath);
		archive.Field("ExportTexture", ExportTexture);
		archive.Field("ExportOnSave", bExportOnSave);
		std::string palette = PaletteToString(Palette);
		archive.Field("Palette", palette);
		if (archive.IsLoading()) {
			Palette = PaletteFromString(palette);
		}
	}

	bool Painting::Load(const std::filesystem::path& file, std::string& outError)
	{
		TextDocument document;
		if (!document.Load(file, outError)) {
			return false;
		}
		TextSection* header = document.FindSection(PAINTINGSECTION);
		if (!header) {
			outError = "Not a painting (no [Painting] section).";
			return false;
		}
		*this = Painting{};
		Palette = DefaultPalette();
		TextArchive headerArchive(*header, ArchiveMode::Load);
		SerializeHeader(headerArchive);
		if (Width <= 0 || Height <= 0 || Width > MaxSize || Height > MaxSize) {
			outError = "Invalid painting size " + std::to_string(Width) + "x" + std::to_string(Height) + ".";
			return false;
		}

		for (TextSection& section : document.Sections) {
			if (section.Type != LAYERSECTION) {
				continue;
			}
			TextArchive archive(section, ArchiveMode::Load);
			archive.SerializeVersion(LayerVersion);
			Layer layer;
			std::string blend = ToString(layer.Blend);
			std::string pixels;
			archive.Field("Name", layer.Name);
			archive.Field("Visible", layer.bIsVisible);
			archive.Field("Locked", layer.bIsLocked);
			archive.Field("Opacity", layer.Opacity);
			archive.Field("Blend", blend);
			archive.Field("Pixels", pixels);
			BlendModeFromString(blend, layer.Blend);
			layer.Opacity = std::clamp(layer.Opacity, 0.0f, 1.0f);

			// A layer that can't be decoded (or has another size) loads empty instead of failing the painting.
			layer.Pixels = std::make_shared<Image>(Width, Height);
			std::vector<uint8_t> png;
			Image decoded;
			std::string error;
			if (!pixels.empty() && DecodeBase64(pixels, png) && DecodePng(png.data(), png.size(), decoded, error)) {
				layer.Pixels->Paste(decoded, 0, 0, false);
			} else if (!pixels.empty()) {
				SDL_Log("%s: layer \"%s\" could not be read%s", PathToUtf8(file).c_str(), layer.Name.c_str(), error.empty() ? "" : (": " + error).c_str());
			}
			Layers.push_back(std::move(layer));
		}
		if (Layers.empty()) {
			Layers.push_back(Layer{ "Layer 1", std::make_shared<Image>(Width, Height) });
		}
		return true;
	}

	bool Painting::Save(const std::filesystem::path& file, std::string& outError) const
	{
		TextDocument document;
		TextArchive headerArchive(document.AddSection(PAINTINGSECTION), ArchiveMode::Save);
		const_cast<Painting*>(this)->SerializeHeader(headerArchive);

		for (const Layer& source : Layers) {
			std::vector<uint8_t> png;
			if (!EncodePng(*source.Pixels, png, outError)) {
				return false;
			}
			Layer layer = source;
			std::string blend = ToString(layer.Blend);
			std::string pixels = EncodeBase64(png);
			TextArchive archive(document.AddSection(LAYERSECTION), ArchiveMode::Save);
			archive.SerializeVersion(LayerVersion);
			archive.Field("Name", layer.Name);
			archive.Field("Visible", layer.bIsVisible);
			archive.Field("Locked", layer.bIsLocked);
			archive.Field("Opacity", layer.Opacity);
			archive.Field("Blend", blend);
			archive.Field("Pixels", pixels);
		}
		return document.Save(file, outError);
	}

	Image& Painting::EditPixels(size_t layerIndex)
	{
		std::shared_ptr<Image>& pixels = Layers[layerIndex].Pixels;
		if (pixels.use_count() > 1) {
			pixels = std::make_shared<Image>(*pixels);
		}
		return *pixels;
	}

	void Painting::Composite(Image& outImage, const PixelRect& rect) const
	{
		const PixelRect clipped = rect.Intersect({ 0, 0, Width, Height });
		for (int y = clipped.Y; y < clipped.Bottom(); ++y) {
			for (int x = clipped.X; x < clipped.Right(); ++x) {
				float backdrop[4] = {};
				for (const Layer& layer : Layers) {
					if (layer.bIsVisible) {
						CompositePixel(backdrop, layer.Pixels->At(x, y), layer.Opacity, layer.Blend);
					}
				}
				outImage.At(x, y) = ToPixel(backdrop);
			}
		}
	}

	Image Painting::Flatten() const
	{
		Image image(Width, Height);
		Composite(image, image.GetRect());
		return image;
	}

	void Painting::MergeDown(size_t layerIndex)
	{
		if (layerIndex == 0 || layerIndex >= Layers.size()) {
			return;
		}
		const Layer upper = Layers[layerIndex];
		Image& lower = EditPixels(layerIndex - 1);
		if (upper.bIsVisible) {
			for (int y = 0; y < Height; ++y) {
				for (int x = 0; x < Width; ++x) {
					const Pixel base = lower.At(x, y);
					const float alpha = base.A / 255.0f;
					float backdrop[4] = { base.R / 255.0f * alpha, base.G / 255.0f * alpha, base.B / 255.0f * alpha, alpha };
					CompositePixel(backdrop, upper.Pixels->At(x, y), upper.Opacity, upper.Blend);
					lower.At(x, y) = ToPixel(backdrop);
				}
			}
		}
		Layers.erase(Layers.begin() + static_cast<std::ptrdiff_t>(layerIndex));
	}
}
