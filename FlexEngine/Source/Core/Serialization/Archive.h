#pragma once
#include "Core/Core.h"
#include "Core/Uid.h"
#include "Core/Math/Color.h"
#include "Core/Math/Vector2.h"
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace Flex
{
	enum class ArchiveMode
	{
		Load,
		Save
	};

	// One Serialize function works for loading AND saving, in any format:
	//
	//     void Texture::Serialize(Archive& archive)
	//     {
	//         const uint32_t version = archive.SerializeVersion(Texture::Version);
	//         archive.Field("Filter", Filter);
	//         if (version >= 2) {
	//             archive.Field("Wrap", Wrap);
	//         }
	//     }
	//
	// Loading never fails hard: a missing or unreadable field keeps its current
	// (default) value, and the problem is recorded in GetWarnings().
	class FLEX_API Archive
	{
	public:
		explicit Archive(ArchiveMode mode) : Mode{ mode } {}
		virtual ~Archive() = default;

		bool IsLoading() const { return Mode == ArchiveMode::Load; }
		bool IsSaving() const { return Mode == ArchiveMode::Save; }

		// Saving: stores currentVersion and returns it.
		// Loading: returns the stored version (0 when the data has none).
		virtual uint32_t SerializeVersion(uint32_t currentVersion) = 0;

		virtual void Field(const char* name, bool& value) = 0;
		virtual void Field(const char* name, int32_t& value) = 0;
		virtual void Field(const char* name, uint32_t& value) = 0;
		virtual void Field(const char* name, int64_t& value) = 0;
		virtual void Field(const char* name, float& value) = 0;
		virtual void Field(const char* name, double& value) = 0;
		virtual void Field(const char* name, std::string& value) = 0;
		virtual void Field(const char* name, Uid& value) = 0;

		// An asset reference that may only point at assets of these types (type names as the
		// asset database knows them, e.g. Texture::TypeName, "Scene"). Stored exactly like a
		// plain Uid field; the editor's Inspector uses the types to filter what can be picked.
		//     archive.Field("Asset", AssetId, { Sprite::TypeName, Texture::TypeName });
		virtual void Field(const char* name, Uid& value, std::initializer_list<const char*> allowedAssetTypes)
		{
			(void)allowedAssetTypes;
			Field(name, value);
		}

		// Groups the fields in between, e.g. "Transform" -> Transform.Position in text.
		virtual void BeginObject(const char* name) = 0;
		virtual void EndObject() = 0;

		// Math types are stored as groups, so every archive supports them for free:
		//     Position.X = 120
		//     Position.Y = 64
		// Virtual so an archive can show them differently (the editor's Inspector does).
		virtual void Field(const char* name, Vector2& value)
		{
			BeginObject(name);
			Field("X", value.x);
			Field("Y", value.y);
			EndObject();
		}

		virtual void Field(const char* name, Vector2Int& value)
		{
			BeginObject(name);
			Field("X", value.x);
			Field("Y", value.y);
			EndObject();
		}

		virtual void Field(const char* name, Color& value)
		{
			BeginObject(name);
			Field("R", value.r);
			Field("G", value.g);
			Field("B", value.b);
			Field("A", value.a);
			EndObject();
		}

		// A list of structs that have their own void Serialize(Archive&), built on Field and BeginObject so
		// every archive supports it:
		//     Bindings.Count = 2
		//     Bindings.0.Action = "Jump"
		//     Bindings.1.Action = "Move"
		// Loading resizes the vector (at most MaxArrayCount elements, so a corrupt count can't eat memory).
		static constexpr uint32_t MaxArrayCount = 65536;

		template<typename T>
		void ArrayField(const char* name, std::vector<T>& values)
		{
			BeginObject(name);
			uint32_t count = static_cast<uint32_t>(values.size());
			Field("Count", count);
			if (IsLoading()) {
				if (count > MaxArrayCount) {
					AddWarning(std::string(name) + ": " + std::to_string(count) + " elements is too many; only " + std::to_string(MaxArrayCount) + " are read.");
					count = MaxArrayCount;
				}
				values.resize(count);
			}
			for (uint32_t index = 0; index < static_cast<uint32_t>(values.size()); ++index) {
				const std::string element = std::to_string(index);
				BeginObject(element.c_str());
				values[index].Serialize(*this);
				EndObject();
			}
			EndObject();
		}

		const std::vector<std::string>& GetWarnings() const { return Warnings; }

	protected:
		void AddWarning(std::string warning) { Warnings.push_back(std::move(warning)); }

	private:
		ArchiveMode Mode;
		std::vector<std::string> Warnings;
	};
}
