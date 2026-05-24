#pragma once
#include "RenderComponent.h"
#include "Texture.h"

class SpriteRenderer : public RenderComponent
{
public:
	DECLARE_COMPONENT(SpriteRenderer, "Sprite Renderer")

	SpriteRenderer();

	void Render() const override;

public:
	Texture Sprite;
};

