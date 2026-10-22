#pragma once
#include "Core/Uid.h"
#include "Core/World/Entity.h"

namespace Flex
{
	// What the user has selected in the editor: an asset (Content window) or a GameObject
	// (Hierarchy, Viewport). The Inspector shows it. One item at a time for now.
	class Selection
	{
	public:
		enum class Kind
		{
			None,
			Asset,
			Entity
		};

		void Clear()
		{
			SelectedKind = Kind::None;
			AssetId = Uid();
			SelectedEntity = Flex::Entity{};
		}

		void SelectAsset(Uid id)
		{
			Clear();
			if (id.IsValid()) {
				SelectedKind = Kind::Asset;
				AssetId = id;
			}
		}

		void SelectEntity(Flex::Entity entity)
		{
			Clear();
			if (entity.IsValid()) {
				SelectedKind = Kind::Entity;
				SelectedEntity = entity;
			}
		}

		Kind GetKind() const { return SelectedKind; }
		Uid GetAsset() const { return SelectedKind == Kind::Asset ? AssetId : Uid(); }
		Flex::Entity GetEntity() const { return SelectedKind == Kind::Entity ? SelectedEntity : Flex::Entity{}; }
		bool IsAssetSelected(Uid id) const { return SelectedKind == Kind::Asset && AssetId == id; }
		bool IsEntitySelected(Flex::Entity entity) const { return SelectedKind == Kind::Entity && SelectedEntity == entity; }

	private:
		Kind SelectedKind = Kind::None;
		Uid AssetId;
		Flex::Entity SelectedEntity;
	};
}
