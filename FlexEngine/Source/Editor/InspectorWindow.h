#pragma once
#include "Editor/EditorCore.h"
#include "Editor/EditorWindow.h"
#include "Core/Assets/Texture.h"
#include "Core/Uid.h"
#include "Core/World/Entity.h"
#include <cstdint>
#include <functional>
#include <string>

namespace Flex
{
	class InspectorArchive;
	struct AssetInfo;
	struct EditorContext;

	// Shows and edits whatever is selected. For assets: name, type, path, id, a preview,
	// and import settings where the type has them (texture filter).
	class InspectorWindow final : public EditorWindow
	{
	public:
		explicit InspectorWindow(EditorContext& context);

		void Draw() override;

		// Draws a component type its own way instead of field by field from Serialize. It runs inside the
		// Inspector's two-column table (label, value) with the component's InspectorArchive, so it can use
		// archive.Field(...) for ordinary rows, add its own rows, and call archive.MarkEdited() after
		// changing the component with its own widgets.
		using ComponentDrawer = void (*)(InspectorArchive& archive, void* component);
		FLEX_EDITOR_API static void RegisterComponentDrawer(const std::string& componentName, ComponentDrawer drawer);

	private:
		void DrawAsset(Uid assetId);
		void DrawEntity(Entity entity);
		void DrawComponents(Entity entity);
		void DrawAddComponent(Entity entity);
		void DrawTextureSettings(const AssetInfo& asset);

	private:
		EditorContext& Context;

		// Import settings of the selected texture, read once per selection/refresh.
		Uid SettingsAssetId;
		uint32_t SettingsRevision = UINT32_MAX;
		TextureImportSettings Settings;
		std::string SettingsError;

		// Name being edited (GameObjectInfo::Name is a std::string; ImGui edits a buffer).
		Entity NameEntity;
		char NameBuffer[128]{};

		// Add/remove change the entity's archetype, so they run after the components are drawn.
		std::function<void()> PendingAction;
		char ComponentFilter[64]{};
	};
}
