#pragma once
#include "SystemRegistery.h"
#include <set>

class Camera;

class CameraManager : public System
{
	DECLARE_SYSTEM(CameraManager)
public:
	CameraManager();
	~CameraManager();

	void Register(Camera* camera);
	void UnRegister(Camera* camera);

	void PromoteToMainCamera(Camera* camera);

	Camera* GetMainCamera();

private:
	Camera* MainCamera;
	std::set<Camera*> Cameras;
};

