#pragma once
#include "Asset.h"
#include <SDL/SDL.h>
#include <imgui/imgui_internal.h>

class TextureManager;
class TextureEditor;

class Texture : public Asset
{
public:
	ASSET_LOOKUP(Texture, "texture", ".png", ".jpeg")

	Texture();
	Texture(SDL_Texture* texture);

	SDL_Texture* Get() const { return sdltexture; }
	SDL_Color GetColor() const { return tint; }
	SDL_Rect GetCropRect() const { return crop; }
	ImVec2i GetSize() const { return size; }

	// Inherited via Asset
	void Load(std::ifstream& inputstream) override;
	void Save(std::ofstream& ouputstream) override;
	void ImportFrom(const std::string& filepath) override;

#ifdef _DEBUG
	void ImGui_ContentHelper() override;
	virtual void ImGui_OnOpen() override;
	virtual  GameObject* CreateGameObject() override;
#endif
	void Refresh();

private:
	SDL_Texture* sdltexture;
	SDL_Color tint;
	SDL_Rect crop;
	ImVec2i size;
	friend TextureManager;
	friend TextureEditor;
};

