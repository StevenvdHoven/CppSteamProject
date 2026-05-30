#include "Scene.h"
#include "Texture.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "BinarySaving.h"

void Scene::Load(std::ifstream& inputstream)
{
	if (&SceneManager::GetCurrentScene() != this) {
		return;
	}
	std::string assetType;
	std::string source;
	BinarySaving::ReadAsset(inputstream, assetType, source);

	std::uint32_t gameobjectsSize = 0;
	inputstream.read(reinterpret_cast<char*>(&gameobjectsSize), sizeof(gameobjectsSize));
	for (int gameObjectIndex{ 0 }; gameObjectIndex < gameobjectsSize; ++gameObjectIndex) {
		auto gameObject = std::make_unique<GameObject>();
		gameObject->Load(inputstream);
		Add(std::move(gameObject));
	}
}

void Scene::Save(std::ofstream& outputstream)
{
	std::uint32_t gameobjectsSize = 0;
	for (const auto& gameObject : GameObjects) {
		if (!gameObject->GetParent()) {
			++gameobjectsSize;
		}
	}
	outputstream.write(reinterpret_cast<const char*>(&gameobjectsSize), sizeof(gameobjectsSize));
	for (const auto& gameObject : GameObjects) {
		if (!gameObject->GetParent()) {
			gameObject->Save(outputstream);
		}
	}
}

void Scene::Start()
{
	for (auto& gameObject : GameObjects) {
		if (!gameObject->GetParent()) {
			gameObject->Start();
		}
	}
}

void Scene::Update(float deltaTime)
{
	for (auto& gameObject : GameObjects) {
		if (!gameObject->GetParent()) {
			gameObject->Update(deltaTime);
		}
	}
}
#ifdef _DEBUG
void Scene::ImGui_ContentHelper()
{
	static Texture SCENEICON;
	if (SCENEICON.Get() == nullptr)
	{
		SCENEICON = TextureManager::LoadTexture("EditorContent/scene_icon.png");
	}

	ImGui::BeginGroup();

	constexpr float IMAGEWIDTH = 100.f;
	float factor = IMAGEWIDTH / (float)SCENEICON.GetSize().x;
	ImVec2 size(IMAGEWIDTH, SCENEICON.GetSize().y * factor);
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImGui::PushID(this);
	if (ImGui::InvisibleButton("##InvisibleButton", size))
	{
		SceneManager::OpenScene(*this);
	}
	ImGui::PopID();
	bool hovered = ImGui::IsItemHovered();

	// Draw image on top of the button area
	ImGui::SetCursorScreenPos(pos);

	ImVec4 tint = hovered
		? ImVec4(0.7f, 0.8f, 1.0f, 1.0f) // slight blue tint
		: ImVec4(1.f, 1.f, 1.f, 1.f);

	ImGui::ImageWithBg((ImTextureID)SCENEICON.Get(), size, ImVec2{0.0f,0.0f}, ImVec2{ 1.0f,1.0f }, ImVec4{255,255,255,255}, tint);

	Asset::ImGui_ContentHelper();
	ImGui::EndGroup();
}
#endif