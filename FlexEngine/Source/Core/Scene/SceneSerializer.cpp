#include "Core/Scene/SceneSerializer.h"
#include "Core/Scene/Scene.h"
#include "Core/Serialization/TextArchive.h"
#include "Core/World/GameObject.h"
#include "Core/World/Hierarchy.h"
#include "Core/World/Transform.h"
#include "Core/World/World.h"
#include <algorithm>
#include <unordered_map>

namespace Flex
{
	FLEX_REGISTER_COMPONENT(PreservedComponents)

	namespace
	{
		constexpr const char* SceneSection = "Scene";
		constexpr const char* GameObjectSection = "GameObject";
	}

	// Transform first (it's the one every GameObject has), then by name, so files diff nicely.
	std::vector<const ComponentTypeInfo*> SceneSerializer::GetSavedComponents(const World& world, Entity entity)
	{
		const ComponentId gameObjectId = GetComponentId<GameObjectInfo>();
		const ComponentId transformId = GetComponentId<Transform>();
		std::vector<const ComponentTypeInfo*> types;
		for (ComponentId id : world.GetComponentIds(entity)) {
			const ComponentTypeInfo& type = ComponentRegistery::GetInfo(id);
			if (type.Serialize && id != gameObjectId) {
				types.push_back(&type);
			}
		}
		std::sort(types.begin(), types.end(), [transformId](const ComponentTypeInfo* a, const ComponentTypeInfo* b) {
			if ((a->Id == transformId) != (b->Id == transformId)) {
				return a->Id == transformId;
			}
			return a->Name < b->Name;
		});
		return types;
	}

	namespace
	{
		void SaveGameObject(Scene& scene, Entity entity, bool bWriteParent, TextDocument& document)
		{
			World& world = scene.GetWorld();
			GameObjectInfo* info = world.GetComponent<GameObjectInfo>(entity);
			if (!info) {
				return;
			}

			TextSection& section = document.AddSection(GameObjectSection);
			TextArchive archive(section, ArchiveMode::Save);
			info->Serialize(archive);
			if (bWriteParent) {
				if (const GameObjectInfo* parentInfo = world.GetComponent<GameObjectInfo>(Hierarchy::GetParent(world, entity))) {
					Uid parentId = parentInfo->Id;
					archive.Field("Parent", parentId);
				}
			}

			for (const ComponentTypeInfo* type : SceneSerializer::GetSavedComponents(world, entity)) {
				SceneSerializer::SaveComponent(world.GetComponentById(entity, type->Id), *type, document.AddSection(type->Name));
			}
			if (const PreservedComponents* preserved = world.GetComponent<PreservedComponents>(entity)) {
				for (const TextSection& kept : preserved->Sections) {
					document.Sections.push_back(kept);
				}
			}

			// Saving makes no structural changes, so the children list stays valid.
			for (Entity child : Hierarchy::GetChildren(world, entity)) {
				SaveGameObject(scene, child, true, document);
			}
		}

		void AddWarnings(std::vector<std::string>& outWarnings, const std::string& prefix, const std::vector<std::string>& warnings)
		{
			for (const std::string& warning : warnings) {
				outWarnings.push_back(prefix + ": " + warning);
			}
		}

		// Reads GameObject sections (and the component sections after each) from first on.
		std::vector<Entity> LoadObjects(Scene& scene, const TextDocument& document, size_t first, std::vector<std::string>& outWarnings)
		{
			World& world = scene.GetWorld();
			std::vector<Entity> topLevel;
			std::unordered_map<Uid, Entity> loaded; // ids in this document -> entities
			Entity current;
			std::string currentName;

			for (size_t index = first; index < document.Sections.size(); ++index) {
				const TextSection& section = document.Sections[index];
				if (section.Type == GameObjectSection) {
					TextSection copy = section; // TextArchive works on a non-const section
					TextArchive archive(copy, ArchiveMode::Load);
					GameObjectInfo info;
					info.Serialize(archive);
					Uid parentId;
					archive.Field("Parent", parentId);

					Entity parent;
					if (parentId.IsValid()) {
						auto it = loaded.find(parentId);
						parent = it != loaded.end() ? it->second : scene.FindByUid(parentId);
					}
					current = scene.CreateGameObject(info.Name, parent, Transform{}, info.Id);
					currentName = info.Name;
					if (!parent.IsValid() || !loaded.contains(parentId)) {
						topLevel.push_back(current);
					}
					if (const GameObjectInfo* created = world.GetComponent<GameObjectInfo>(current)) {
						loaded[info.Id.IsValid() ? info.Id : created->Id] = current;
						if (info.Id.IsValid() && created->Id != info.Id) {
							outWarnings.push_back("GameObject '" + info.Name + "': duplicate id " + info.Id.ToString() + ", given a new one");
						}
					}
					// Only real problems are worth reporting; a root simply has no Parent key.
					std::vector<std::string> warnings;
					for (const std::string& warning : archive.GetWarnings()) {
						if (warning.find("Parent") == std::string::npos) {
							warnings.push_back(warning);
						}
					}
					AddWarnings(outWarnings, "GameObject '" + info.Name + "'", warnings);
					continue;
				}

				if (!current.IsValid()) {
					outWarnings.push_back("Section [" + section.Type + "] before the first GameObject was ignored");
					continue;
				}

				const ComponentTypeInfo* type = ComponentRegistery::FindByName(section.Type);
				if (!type || !type->Serialize || !type->DefaultConstruct) {
					// Unknown here (e.g. a game module that isn't loaded): keep it for saving.
					PreservedComponents* preserved = world.GetComponent<PreservedComponents>(current);
					if (!preserved) {
						preserved = world.AddComponent<PreservedComponents>(current);
					}
					preserved->Sections.push_back(section);
					outWarnings.push_back("GameObject '" + currentName + "': unknown component [" + section.Type + "] kept as it is");
					continue;
				}
				void* component = world.AddComponentById(current, type->Id);
				std::vector<std::string> warnings;
				SceneSerializer::LoadComponent(component, *type, section, warnings);
				AddWarnings(outWarnings, "GameObject '" + currentName + "' [" + type->Name + "]", warnings);
			}

			UpdateWorldTransforms(world);
			return topLevel;
		}
	}

	void SceneSerializer::SaveComponent(void* component, const ComponentTypeInfo& type, TextSection& outSection)
	{
		outSection.Type = type.Name;
		TextArchive archive(outSection, ArchiveMode::Save);
		type.Serialize(component, archive);
	}

	void SceneSerializer::LoadComponent(void* component, const ComponentTypeInfo& type, const TextSection& section, std::vector<std::string>& outWarnings)
	{
		TextSection copy = section;
		TextArchive archive(copy, ArchiveMode::Load);
		type.Serialize(component, archive);
		outWarnings.insert(outWarnings.end(), archive.GetWarnings().begin(), archive.GetWarnings().end());
	}

	void SceneSerializer::Save(Scene& scene, TextDocument& outDocument)
	{
		outDocument = TextDocument{};
		outDocument.AddSection(SceneSection).SetAttribute("version", std::to_string(SceneVersion));
		const std::vector<Entity> roots = scene.GetRoots();
		for (Entity root : roots) {
			SaveGameObject(scene, root, false, outDocument);
		}
	}

	bool SceneSerializer::Load(Scene& scene, const TextDocument& document, std::vector<std::string>& outWarnings, std::string& outError)
	{
		if (document.Sections.empty() || document.Sections[0].Type != SceneSection) {
			outError = "Not a scene file (the first section must be [Scene]).";
			return false;
		}
		scene.Clear();
		LoadObjects(scene, document, 1, outWarnings);
		return true;
	}

	bool SceneSerializer::SaveToFile(Scene& scene, const std::filesystem::path& file, std::string& outError)
	{
		TextDocument document;
		Save(scene, document);
		return document.Save(file, outError);
	}

	bool SceneSerializer::LoadFromFile(Scene& scene, const std::filesystem::path& file, std::vector<std::string>& outWarnings, std::string& outError)
	{
		TextDocument document;
		if (!document.Load(file, outError)) {
			return false;
		}
		return Load(scene, document, outWarnings, outError);
	}

	void SceneSerializer::SaveGameObjects(Scene& scene, std::span<const Entity> roots, TextDocument& outDocument)
	{
		outDocument = TextDocument{};
		for (Entity root : roots) {
			SaveGameObject(scene, root, true, outDocument);
		}
	}

	std::vector<Entity> SceneSerializer::LoadGameObjects(Scene& scene, const TextDocument& document, std::vector<std::string>& outWarnings)
	{
		return LoadObjects(scene, document, 0, outWarnings);
	}
}
