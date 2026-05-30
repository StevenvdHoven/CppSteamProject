#pragma once
#include "Matrix2x3.h"
#include "Component.h"
#include <SDL/SDL.h>

class Camera : public Component
{
public:
	DECLARE_COMPONENT(Camera, "Camera")

	Camera();
	virtual ~Camera() = default;

	void PromoteToMainCamera();

private:
	SDL_Color BackgroundColor;
};

