#include "SpriteRenderer.h"
#include "Renderer.h"
#include "AssetManager.h"
#include "Transform.h"

SpriteRenderer::SpriteRenderer():
	RenderComponent()
{
	
}

void SpriteRenderer::Load(std::ifstream& inputstream)
{
	RenderComponent::Load(inputstream);
	Sprite.Load(inputstream);
}

void SpriteRenderer::Save(std::ofstream& outputstream)
{
	RenderComponent::Save(outputstream);
	Sprite.Save(outputstream);
}

void SpriteRenderer::Render()
{
	if (Texture* tex = Sprite.Get()) {
		Renderer::RenderTexture(*tex, GetTransform());
	}
	
}
#ifdef _DEBUG
void SpriteRenderer::ImGui_OnWindow()
{
	RenderComponent::ImGui_OnWindow();

	Sprite.ImGui_OnDrawProperty("Sprite");
}
#endif