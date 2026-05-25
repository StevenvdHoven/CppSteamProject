#pragma once
#include "AssetManager.h"
#include "RenderComponent.h"
#include "Texture.h"

class SpriteRenderer : public RenderComponent
{
public:
	DECLARE_COMPONENT(SpriteRenderer, "Sprite Renderer")

	SpriteRenderer();

	virtual void Load(std::ifstream& inputstream) override;
	virtual void Save(std::ofstream& outputstream) override;

	void Render() override;

#ifdef _DEBUG
	virtual void ImGui_OnWindow();
#endif // _DEBUG

public:
	AssetProperty<Texture> Sprite;
};

