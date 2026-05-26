#ifdef _DEBUG
#include "TextureEditor.h"
#include "AssetManager.h"
#include "Texture.h"
#include <SDL/SDL.h>

void TextureEditor::Open(Texture* texture)
{
	bIsHidden = false;
	if (texture == nullptr) {
		Close();
		return;
	}
	LoadedTexture = texture;
	EditTexture = *LoadedTexture;
}

void TextureEditor::Close()
{
	bIsHidden = true;
	if (!LoadedTexture)
	{
		return;
	}

	EditTexture = Texture();
}

void TextureEditor::ImGui_OnWindow()
{
	
	ImGui::Begin("Texture Editor");
	constexpr float IMAGEWIDTH = 500.f;
	
	int width;
	int height;
	SDL_QueryTexture(EditTexture.Get(), nullptr, nullptr, &width, &height);

	
	float factor = IMAGEWIDTH / (float)EditTexture.crop.w;

	ImVec2 size(
		IMAGEWIDTH,
		EditTexture.crop.h * factor
	);

	ImGui::BeginGroup();
	ImVec4 tintRef(
		EditTexture.tint.r / 255.0f,
		EditTexture.tint.g / 255.0f,
		EditTexture.tint.b / 255.0f,
		EditTexture.tint.a / 255.0f
	);

	ImVec2 uv0(
		EditTexture.crop.x / (float)EditTexture.size.x,
		EditTexture.crop.y / (float)EditTexture.size.y
	);

	ImVec2 uv1(
		(EditTexture.crop.x + EditTexture.crop.w) / (float)EditTexture.size.x,
		(EditTexture.crop.y + EditTexture.crop.h) / (float)EditTexture.size.y
	);

	
	ImGui::InvisibleButton("##crop_area", size);

	ImVec2 imageMin = ImGui::GetItemRectMin();
	ImVec2 imageMax = ImGui::GetItemRectMax();

	bool hovered = ImGui::IsItemHovered();
	bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	bool isSelecting = false;
	ImVec2 start = imageMin;
	ImVec2 end = imageMax;
	ImGui::SetCursorScreenPos(imageMin);
	Helper_CropImage(hovered, clicked, imageMin, imageMax, size, isSelecting, start, end);
	ImGui::ImageWithBg((ImTextureID)EditTexture.Get(), size, uv0, uv1, ImVec4{255,255,255,255}, tintRef);
	if (isSelecting) {
		ImDrawList* dl = ImGui::GetWindowDrawList();

		dl->AddRect(start, end, IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);
		dl->AddRectFilled(start, end, IM_COL32(255, 0, 0, 50));
	}

	ImGui::EndGroup();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	ImGui::BeginGroup();
	ImGui::Text("Texture Values");
	ImGui::Separator();
	ImGui::Text("Name: %s", EditTexture.GetName().c_str());
	ImGui::Text("Source: %s", EditTexture.GetSource().c_str());
	ImGui::Text("Path: %s", EditTexture.GetPath().c_str());
	ImGui::Separator();
	ImGui::Text("Texture Parameters");
	ImGui::SetNextItemWidth(150.0f);
	ImGui::ColorEdit4("Tint", &tintRef.x);
	EditTexture.tint = SDL_Color(tintRef.x * 255.f, tintRef.y * 255.f, tintRef.z * 255.f, tintRef.w * 255.f);
	ImGui::SetNextItemWidth(100.0f);
	ImGui::InputInt2("Position", &EditTexture.crop.x);
	ImGui::SetNextItemWidth(100.0f);
	ImGui::InputInt2("Size", &EditTexture.crop.w);

	EditTexture.crop.x = std::clamp(EditTexture.crop.x, 0, EditTexture.size.x - 1);
	EditTexture.crop.y = std::clamp(EditTexture.crop.y, 0, EditTexture.size.y - 1);

	EditTexture.crop.w = std::clamp(EditTexture.crop.w, 1, EditTexture.size.x - EditTexture.crop.x);
	EditTexture.crop.h = std::clamp(EditTexture.crop.h, 1, EditTexture.size.y - EditTexture.crop.y);
	ImGui::Separator();
	
	ImGui::Separator();
	if (ImGui::Button("Reset Crop"))
	{
		EditTexture.crop.x = 0;
		EditTexture.crop.y = 0;
		EditTexture.crop.w = EditTexture.size.x;
		EditTexture.crop.h = EditTexture.size.y;

	}
	ImGui::Separator();
	if (ImGui::Button("Save"))
	{
		Save();
	}

	if (ImGui::Button("Save as duplicate"))
	{
		Save(true);
	}
	
	ImGui::EndGroup();

	if (ImGui::IsWindowCollapsed())
	{
		ImGui::SetWindowCollapsed(false);
		Close();
	}

	ImGui::End();
}

void TextureEditor::Helper_CropImage(bool hovered, bool clicked, const ImVec2& imageMin, const ImVec2& imageMax, const ImVec2& size, bool& isSelecting, ImVec2& start, ImVec2& end)
{
	static bool selecting = false;
	static ImVec2 localStart;

	ImVec2 mousepos = ImGui::GetMousePos();
	mousepos.x = std::clamp(mousepos.x, imageMin.x, imageMax.x);
	mousepos.y = std::clamp(mousepos.y, imageMin.y, imageMax.y);

	if (hovered && clicked && !selecting)
	{
		selecting = true;
		localStart = mousepos;
	}

	start = localStart;
	end = mousepos;
	

	if (selecting)
	{
		ImVec2 localStart = ImVec2(start.x - imageMin.x, start.y - imageMin.y);
		ImVec2 localEnd = ImVec2(end.x - imageMin.x, end.y - imageMin.y);

		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			selecting = false;

			ImVec2 min(
				std::min(localStart.x, localEnd.x),
				std::min(localStart.y, localEnd.y)
			);

			ImVec2 max(
				std::max(localStart.x, localEnd.x),
				std::max(localStart.y, localEnd.y)
			);

			float sx = EditTexture.size.x / size.x;
			float sy = EditTexture.size.y / size.y;

			EditTexture.crop.x = (int)(min.x * sx);
			EditTexture.crop.y = (int)(min.y * sy);
			EditTexture.crop.w = (int)((max.x - min.x) * sx);
			EditTexture.crop.h = (int)((max.y - min.y) * sy);
		}
	}

	ImVec2 startMin(
		std::min(start.x, end.x),
		std::min(start.y, end.y)
	);

	ImVec2 endMax(
		std::max(start.x, end.x),
		std::max(start.y, end.y)
	);

	start = startMin;
	end = endMax;

	isSelecting = selecting;
}

void TextureEditor::Save(bool isDuplicate)
{
	if (!isDuplicate)
	{
		AssetManager::SaveAsset(&EditTexture);
		*LoadedTexture = EditTexture;
		return;
	}

	Texture temp = EditTexture;
	AssetManager::SaveAsset(&temp, true);
	
}
#endif