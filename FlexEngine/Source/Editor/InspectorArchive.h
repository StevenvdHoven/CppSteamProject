#pragma once
#include "Editor/EditorCore.h"
#include "Core/Serialization/Archive.h"
#include <initializer_list>
#include <string>
#include <vector>

namespace Flex
{
	struct EditorContext;

	// Draws a component's fields by running its Serialize(Archive&) through this archive:
	// every Field() becomes a widget that edits the value in place. So any component that
	// can be saved shows up in the Inspector without extra code.
	//
	// Use inside a 2-column table (label | widget). Until FLEX_PROPERTY metadata exists,
	// float fields named "Rotation" are shown in degrees (stored in radians).
	class FLEX_EDITOR_API InspectorArchive final : public Archive
	{
	public:
		explicit InspectorArchive(EditorContext& context);

		using Archive::Field;

		uint32_t SerializeVersion(uint32_t currentVersion) override { return currentVersion; }

		void Field(const char* name, bool& value) override;
		void Field(const char* name, int32_t& value) override;
		void Field(const char* name, uint32_t& value) override;
		void Field(const char* name, int64_t& value) override;
		void Field(const char* name, float& value) override;
		void Field(const char* name, double& value) override;
		void Field(const char* name, std::string& value) override;
		void Field(const char* name, Uid& value) override;
		void Field(const char* name, Uid& value, std::initializer_list<const char*> allowedAssetTypes) override;
		void Field(const char* name, Vector2& value) override;
		void Field(const char* name, Vector2Int& value) override;
		void Field(const char* name, Color& value) override;

		void BeginObject(const char* name) override;
		void EndObject() override;

		bool WasChanged() const { return bChanged; }            // a value changed this frame
		bool WasEditStarted() const { return bEditStarted; }     // a widget became active this frame
		bool WasEditFinished() const { return bEditFinished; }   // a widget finished an edit this frame

	private:
		void BeginRow(const char* name);
		void EndRow(bool bValueChanged);
		// allowedAssetTypes empty = any asset.
		void AssetSlot(const char* name, Uid& value, std::initializer_list<const char*> allowedAssetTypes);
		// The popup list: search box, "None", and every asset of the allowed types. True when one was picked.
		bool AssetPicker(Uid& value, std::initializer_list<const char*> allowedAssetTypes);

	private:
		EditorContext& Context;
		std::vector<std::string> Groups;
		bool bChanged = false;
		bool bEditStarted = false;
		bool bEditFinished = false;
	};
}
