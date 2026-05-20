#include "TextureEditor.h"
#include "Texture.h"
#include <SDL/SDL.h>

void TextureEditor::Open(Texture* texture)
{
	bIsHidden = false;
	if (texture == nullptr) {
		Close();
		return;
	}

	CurrentLoadedTexture = texture;
}

void TextureEditor::Close()
{
	bIsHidden = true;
	if (!CurrentLoadedTexture)
	{
		return;
	}
	CurrentLoadedTexture = nullptr;
}

void TextureEditor::ImGui_OnWindow()
{
	ImGui::Begin("Texture Editor");
	ImGui::BeginGroup();
	int width;
	int height;
	SDL_QueryTexture(CurrentLoadedTexture->Get(), nullptr, nullptr, &width, &height);

	constexpr float IMAGEWIDTH = 600.f;
	float factor = IMAGEWIDTH / std::max(1.0f, static_cast<float>(width));
	ImVec2 size(IMAGEWIDTH, height * factor);
	ImVec4 tintRef(CurrentLoadedTexture->tint.r, CurrentLoadedTexture->tint.g, CurrentLoadedTexture->tint.b, CurrentLoadedTexture->tint.a);

	ImVec2 uv0(0, 0);
	ImVec2 uv1(1, 1);

	ImGui::Image((ImTextureID)CurrentLoadedTexture->Get(), size, uv0, uv1, tintRef, ImVec4{});
	ImGui::EndGroup();
	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::Text("Texture Values");
	ImGui::Separator();
	ImGui::Text("Name: %s", CurrentLoadedTexture->GetName().c_str());
	ImGui::Text("Source: %s", CurrentLoadedTexture->GetSource().c_str());
	ImGui::Text("Path: %s", CurrentLoadedTexture->GetPath().c_str());
	ImGui::Separator();
	ImGui::Text("Texture Parameters");
	ImGui::ColorEdit4("Tint", &tintRef.x);
	CurrentLoadedTexture->tint = SDL_Color(tintRef.x, tintRef.y, tintRef.z, tintRef.w);
	ImGui::InputFloat2("UV0", &uv0.x);

	ImGui::Separator();
	
	ImGui::Separator();
	if (ImGui::Button("Crop Image"))
	{

	}
	ImGui::EndGroup();

	if (ImGui::IsWindowCollapsed())
	{
		ImGui::SetWindowCollapsed(false);
		Close();
	}

	ImGui::End();
}
