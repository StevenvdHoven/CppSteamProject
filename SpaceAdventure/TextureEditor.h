#ifdef _DEBUG
#pragma once
#include "EditorWindow.h"
#include "Texture.h"

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

	void Helper_CropImage(bool hovered, bool clicked, const ImVec2& imageMin,const ImVec2& imageMax, const ImVec2& size, bool& isSelecting, ImVec2& start, ImVec2& end);
	void Save(bool isDuplicate = false);

private:
	Texture* LoadedTexture;
	Texture EditTexture;

	bool bIsSelecting = false;
};
#endif