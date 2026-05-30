#include "CameraManager.h"

CameraManager::CameraManager():
    MainCamera{nullptr}
{
}

CameraManager::~CameraManager()
{
}

void CameraManager::Register(Camera* camera)
{
    Cameras.insert(camera);

    if (MainCamera == nullptr)
    {
        MainCamera = camera;
    }
}

void CameraManager::UnRegister(Camera* camera)
{
    if (Cameras.contains(camera)) {
        Cameras.erase(camera);
    }
    if (MainCamera == camera) {
        MainCamera = nullptr;
        if (Cameras.size()) {
            MainCamera = *Cameras.begin();
        }
    }
}

void CameraManager::PromoteToMainCamera(Camera* camera)
{
    MainCamera = camera;
}

Camera* CameraManager::GetMainCamera()
{
    return MainCamera;
}
