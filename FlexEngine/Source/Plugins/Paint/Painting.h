#pragma once
#include "Plugins/Paint/PaintImage.h"
#include "Core/Uid.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Flex
{
	class Archive;
}

namespace Flex::Paint
{
	enum class BlendMode : uint8_t
	{
		Normal,
		Multiply,
		Add,
		Screen,
	};
	const char* ToString(BlendMode mode);
	bool BlendModeFromString(const std::string& text, BlendMode& outMode);
	inline constexpr BlendMode AllBlendModes[] = { BlendMode::Normal, BlendMode::Multiply, BlendMode::Add, BlendMode::Screen };

	struct Layer
	{
		std::string Name;
		// Shared with the undo history: copy it before changing pixels (Painting::EditPixels).
		std::shared_ptr<Image> Pixels;
		bool bIsVisible = true;
		bool bIsLocked = false;
		float Opacity = 1.0f;
		BlendMode Blend = BlendMode::Normal;
	};

	// A layered image: the source of a texture, edited in the Paint window and saved as a .flexpaint
	// file in Content/ (FlexEngine text format, editor-only: never copied into builds).
	//     [Painting version=1]
	//     Width = 64
	//     Height = 64
	//     ExportPath = "Characters/Hero.png"   ; relative to Content/
	//     ExportTexture = uid(...)              ; the exported texture, once exported
	//     ExportOnSave = true
	//     Palette = "1a1c2cff 5d275dff ..."     ; RGBA hex per colour
	//     [Layer version=1]                     ; bottom to top
	//     Name = "Outline"
	//     Visible = true
	//     Locked = false
	//     Opacity = 1
	//     Blend = "Normal"
	//     Pixels = "iVBORw0KGgo..."              ; the layer as a PNG, base64
	struct Painting
	{
		static constexpr const char* TypeName = "Painting";
		static constexpr const char* FileExtension = ".flexpaint";
		static constexpr uint32_t Version = 1;
		static constexpr uint32_t LayerVersion = 1;
		static constexpr int MaxSize = 4096;

		int Width = 0;
		int Height = 0;
		std::vector<Layer> Layers;
		std::string ExportPath;
		Uid ExportTexture;
		bool bExportOnSave = true;
		std::vector<Pixel> Palette;

		static Painting Create(int width, int height, Pixel background);
		static std::vector<Pixel> DefaultPalette();

		bool Load(const std::filesystem::path& file, std::string& outError);
		bool Save(const std::filesystem::path& file, std::string& outError) const;

		// The layer's pixels, copied first when the undo history still shares them.
		Image& EditPixels(size_t layerIndex);

		// The visible layers blended together (only rect is written; the rest of outImage stays as it is).
		void Composite(Image& outImage, const PixelRect& rect) const;
		Image Flatten() const;
		// The layer at layerIndex blended onto the one below; the result keeps the lower layer's settings.
		void MergeDown(size_t layerIndex);

	private:
		void SerializeHeader(Archive& archive);
	};
}
