#pragma once
#include "Core/Core.h"
#include "Core/Serialization/TextDocument.h"
#include "Core/World/ComponentRegistery.h"
#include "Core/World/Entity.h"
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace Flex
{
	class Scene;
	class World;

	// Component sections the loader didn't recognise (e.g. from a game module that isn't
	// loaded). Kept on the GameObject and written back unchanged, so no data is lost.
	struct PreservedComponents
	{
		FLEX_COMPONENT(PreservedComponents)
		std::vector<TextSection> Sections;
	};

	// Scene files (.scene) in the FlexEngine text format:
	//
	//     [Scene version=1]
	//
	//     [GameObject version=1]
	//     Name = "Player"
	//     Id = uid(3b22a53519c4b9de)
	//     Parent = uid(...)            ; only for children
	//     [Transform version=1]
	//     Position.X = 120
	//     ...
	//     [SpriteRenderer version=1]
	//     Asset = uid(...)
	//
	// GameObjects are written depth-first in Hierarchy order, each followed by its components
	// (every component type with a Serialize function). Loading rebuilds the same order.
	namespace SceneSerializer
	{
		constexpr uint32_t SceneVersion = 1;
		constexpr const char* FileExtension = ".scene";
		constexpr const char* AssetTypeName = "Scene"; // the asset type of .scene files

		// The entity's components that have a Serialize function (except GameObjectInfo):
		// Transform first, then by name. What scene files save and the Inspector shows.
		FLEX_API std::vector<const ComponentTypeInfo*> GetSavedComponents(const World& world, Entity entity);

		FLEX_API void Save(Scene& scene, TextDocument& outDocument);

		// Replaces the scene's contents. Problems (unknown fields, bad values) become warnings;
		// only a document that isn't a scene at all fails.
		FLEX_API bool Load(Scene& scene, const TextDocument& document, std::vector<std::string>& outWarnings, std::string& outError);

		FLEX_API bool SaveToFile(Scene& scene, const std::filesystem::path& file, std::string& outError);
		FLEX_API bool LoadFromFile(Scene& scene, const std::filesystem::path& file, std::vector<std::string>& outWarnings, std::string& outError);

		// The given GameObjects and everything below them (for undo, later copy/paste/prefabs).
		// The first object's Parent is written too, so it can be restored in place.
		FLEX_API void SaveGameObjects(Scene& scene, std::span<const Entity> roots, TextDocument& outDocument);
		// Adds the GameObjects of such a document (same Uids) to the scene and returns the
		// top-level ones. An object whose parent isn't in the scene becomes a root.
		FLEX_API std::vector<Entity> LoadGameObjects(Scene& scene, const TextDocument& document, std::vector<std::string>& outWarnings);

		// One component as a section (for undo of Inspector edits) and back.
		FLEX_API void SaveComponent(void* component, const ComponentTypeInfo& type, TextSection& outSection);
		FLEX_API void LoadComponent(void* component, const ComponentTypeInfo& type, const TextSection& section, std::vector<std::string>& outWarnings);
	}
}
