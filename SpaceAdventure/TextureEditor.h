#pragma once
#include "EditorWindow.h"

class Texture;

class TextureEditor : public EditorWindow
{
public:
	ASSET_EDITOR("texture")

	TextureEditor() :
		EditorWindow()
	{
		bIsHidden = true;
	}
	
	void Open(Texture* texture);
	void Close();

	// Editor window overrides
	void ImGui_OnWindow() override;
private:
	Texture* CurrentLoadedTexture = nullptr;
};

