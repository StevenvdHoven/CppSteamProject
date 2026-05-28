#pragma once
#include "Component.h"
#include <SDL/SDL.h>

class Camera : public Component
{
public:
	DECLARE_COMPONENT(Camera, "Camera")

	Camera();
	virtual ~Camera() = default;

private:
	SDL_Color BackgroundColor;
};

