#include "RenderComponent.h"
#include "Renderer.h"

RenderComponent::RenderComponent()
{
	Renderer::Register(this);
}

RenderComponent::~RenderComponent()
{
	Renderer::UnRegister(this);
}
