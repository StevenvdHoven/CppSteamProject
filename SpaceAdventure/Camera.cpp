#include "Camera.h"
#include "CameraManager.h"
#include "SystemRegistery.h"

Camera::Camera()
{
	SystemLocator::GetSystem<CameraManager>()->Register(this);
}

