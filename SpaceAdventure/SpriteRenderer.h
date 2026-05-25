#pragma once
#include "RenderComponent.h"
#include "Texture.h"

class SpriteRenderer : public RenderComponent
{
public:
	DECLARE_COMPONENT(SpriteRenderer, "Sprite Renderer")

	SpriteRenderer();

	void Render() const override;

#ifdef _DEBUG
	virtual void ImGui_OnWindow();
#endif // _DEBUG

public:
	Texture Sprite;
};

