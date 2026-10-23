#include "Plugins/Paint/PaintEditor.h"
#include "Plugins/Paint/Painting.h"
#include "Editor/AssetDatabase.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorPlugin.h"
#include "Core/Assets/Texture.h"
#include "Core/FileSystem.h"

#include <imgui.h>
#include <cstdio>

namespace Flex::Paint
{
	namespace
	{
		constexpr const char* NEWPAINTINGPOPUP = "New Painting";
	}

	// The 2D Paint plugin: a "Painting" asset type (.flexpaint, editor-only) with its own editor
	// window, created from Content (right-click > Create) or Assets > New Painting.
	class PaintPlugin final : public EditorPlugin
	{
	public:
		const char* GetName() const override { return "Paint"; }

		void OnLoad(EditorPluginHost& host) override
		{
			Host = &host;
			EditorContext& context = host.GetContext();
			host.RegisterAssetType(Painting::TypeName, { Painting::FileExtension }, {}, AssetTypeFlags::EditorOnly);
			host.RegisterAssetOpener(Painting::TypeName, [this, &context](const AssetInfo& asset) {
				const Uid paintingId = asset.Id;
				Host->OpenAssetEditor(paintingId, {}, [this, &context, paintingId]() {
					return std::make_unique<PaintEditor>(context, Host->GetMainWindow(), paintingId);
				});
			});
			host.RegisterThumbnail(Painting::TypeName, [&context](const AssetInfo& asset) -> std::shared_ptr<Texture> {
				Painting painting;
				std::string error;
				if (!painting.Load(context.Assets.GetSourcePath(asset), error)) {
					return nullptr;
				}
				const Image flat = painting.Flatten();
				return Texture::CreateFromPixels(flat.Width, flat.Height, flat.Data(), TextureFilter::PixelArt, error);
			});
			host.AddMenuItem("Assets", "New Painting...", "", [this, &context]() { OpenNewPaintingDialog(context.Assets.GetContentDirectory(), {}); });
			host.AddContentCreateItem("Painting...", [this](const std::filesystem::path& folder) { OpenNewPaintingDialog(folder, {}); });
			host.AddDrawCallback([this]() { DrawNewPaintingDialog(); });
		}

	private:
		void OpenNewPaintingDialog(const std::filesystem::path& folder, Uid sourceTexture)
		{
			Folder = folder;
			SourceTexture = sourceTexture;
			Error.clear();
			std::snprintf(Name, sizeof(Name), "%s", "NewPainting");
			bRequestOpen = true;
		}

		void DrawNewPaintingDialog()
		{
			if (bRequestOpen) {
				ImGui::OpenPopup(NEWPAINTINGPOPUP);
				bRequestOpen = false;
			}
			if (!ImGui::BeginPopupModal(NEWPAINTINGPOPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				return;
			}
			EditorContext& context = Host->GetContext();
			AssetDatabase& database = context.Assets;
			std::error_code error;
			const std::filesystem::path relativeFolder = std::filesystem::relative(Folder, database.GetContentDirectory(), error);
			ImGui::Text("In Content/%s", PathToGenericUtf8(relativeFolder == "." ? std::filesystem::path() : relativeFolder).c_str());
			ImGui::SetNextItemWidth(240.0f);
			ImGui::InputText("Name", Name, sizeof(Name));

			ImGui::Checkbox("Start from a texture", &bFromTexture);
			if (bFromTexture) {
				const AssetInfo* texture = SourceTexture.IsValid() ? database.Find(SourceTexture) : nullptr;
				ImGui::Text("Texture: %s", texture ? PathToGenericUtf8(texture->Path).c_str() : "(choose one)");
				Uid chosen;
				if (DrawTexturePicker(context, Search, sizeof(Search), chosen)) {
					SourceTexture = chosen;
				}
				ImGui::TextDisabled("The texture becomes the first layer, and exporting overwrites it\n(same asset id, so its sprites and references keep working).");
			} else {
				ImGui::SetNextItemWidth(120.0f);
				ImGui::InputInt("Width", &Width);
				ImGui::SetNextItemWidth(120.0f);
				ImGui::InputInt("Height", &Height);
				Width = std::clamp(Width, 1, Painting::MaxSize);
				Height = std::clamp(Height, 1, Painting::MaxSize);
				for (const int size : { 16, 32, 64, 128, 256 }) {
					ImGui::SameLine();
					if (ImGui::SmallButton(std::to_string(size).c_str())) {
						Width = Height = size;
					}
				}
				ImGui::Combo("Background", &Background, "Transparent\0White\0Black\0");
			}
			if (!Error.empty()) {
				ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", Error.c_str());
			}
			if (ImGui::Button("Create")) {
				if (Create(context)) {
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		bool Create(EditorContext& context)
		{
			AssetDatabase& database = context.Assets;
			const std::string name = Name;
			if (name.empty() || name.find_first_of("\\/:*?\"<>|") != std::string::npos) {
				Error = "Enter a file name without \\ / : * ? \" < > |.";
				return false;
			}
			const std::filesystem::path file = Folder / Utf8ToPath(name + Painting::FileExtension);
			std::error_code fileError;
			if (std::filesystem::exists(file, fileError)) {
				Error = name + Painting::FileExtension + " already exists here.";
				return false;
			}
			const std::filesystem::path relativeFolder = std::filesystem::relative(Folder, database.GetContentDirectory(), fileError);

			Painting painting;
			if (bFromTexture) {
				const AssetInfo* texture = SourceTexture.IsValid() ? database.Find(SourceTexture) : nullptr;
				if (!texture) {
					Error = "Choose a texture.";
					return false;
				}
				Image image;
				if (!LoadImageFile(database.GetSourcePath(*texture), image, Error)) {
					return false;
				}
				if (image.Width > Painting::MaxSize || image.Height > Painting::MaxSize) {
					Error = "The texture is bigger than " + std::to_string(Painting::MaxSize) + " pixels.";
					return false;
				}
				painting = Painting::Create(image.Width, image.Height, {});
				painting.Layers[0].Name = PathToUtf8(texture->Path.stem());
				painting.Layers[0].Pixels = std::make_shared<Image>(std::move(image));
				painting.ExportPath = PathToGenericUtf8(texture->Path);
				painting.ExportTexture = texture->Id;
			} else {
				const Pixel background = Background == 1 ? Pixel{ 255, 255, 255, 255 } : Background == 2 ? Pixel{ 0, 0, 0, 255 } : Pixel{};
				painting = Painting::Create(Width, Height, background);
				painting.ExportPath = PathToGenericUtf8((relativeFolder == "." ? std::filesystem::path() : relativeFolder) / Utf8ToPath(name + ".png"));
			}
			if (!painting.Save(file, Error)) {
				return false;
			}

			context.RefreshAssets();
			const std::filesystem::path relativeFile = std::filesystem::relative(file, database.GetContentDirectory(), fileError);
			if (const AssetInfo* asset = database.FindByPath(relativeFile)) {
				context.OpenAsset(asset->Id);
			}
			return true;
		}

	private:
		EditorPluginHost* Host = nullptr;

		// New Painting dialog
		bool bRequestOpen = false;
		std::filesystem::path Folder;
		char Name[64]{};
		int Width = 32;
		int Height = 32;
		int Background = 0;
		bool bFromTexture = false;
		Uid SourceTexture;
		char Search[64]{};
		std::string Error;
	};
}

FLEX_EDITOR_PLUGIN(Flex::Paint::PaintPlugin)
