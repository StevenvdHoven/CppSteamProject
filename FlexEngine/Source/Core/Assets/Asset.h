#pragma once
#include "Core/Core.h"
#include "Core/Uid.h"
#include <string>

namespace Flex
{
	// Base of everything AssetManager can load (textures now; sprites, scenes, sounds later).
	// Assets are shared: hold them as std::shared_ptr and never copy them.
	class FLEX_API Asset
	{
	public:
		virtual ~Asset() = default;

		Asset(const Asset&) = delete;
		Asset& operator=(const Asset&) = delete;

		Uid GetId() const { return Id; }
		const std::string& GetName() const { return Name; }

		virtual const char* GetTypeName() const = 0;

		// Rough memory use, shown in the editor's statistics.
		virtual size_t GetMemorySize() const { return 0; }

	protected:
		Asset() = default;

	private:
		Uid Id;
		std::string Name;

		friend class AssetManager;
	};
}
