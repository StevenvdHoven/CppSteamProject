#pragma once
#include "Core/Core.h"
#include <concepts>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace Flex
{
	class Archive;

	// Dense id of a component type, the same in every module of this process.
	// Ids depend on registration order, so they are never saved; names are.
	using ComponentId = uint32_t;
	inline constexpr ComponentId InvalidComponentId = UINT32_MAX;

	// Marks a struct as a component and gives it its stable name:
	//
	//     struct Velocity
	//     {
	//         FLEX_COMPONENT(Velocity)
	//         Vector2 Value;
	//     };
	//
	// The name (not the C++ type) identifies the component, so the game module and
	// FlexCore agree on ids even though each DLL has its own template statics.
	#define FLEX_COMPONENT(TypeName) static constexpr const char* ComponentName = #TypeName;

	// Registers a component type when the module loads (put it in one .cpp file), so it can be
	// found by name before any code used it: scene loading and the Inspector's "Add Component".
	#define FLEX_REGISTER_COMPONENT(Type) \
		static const bool FlexComponentRegistered_##Type = (::Flex::GetComponentType<Type>(), true);

	template<typename T>
	concept IsComponent = requires { { T::ComponentName } -> std::convertible_to<const char*>; }
		&& !std::is_const_v<T> && !std::is_reference_v<T>
		&& std::is_move_constructible_v<T> && std::is_destructible_v<T>;

	// Type-erased description of a component type. Lives as long as the process,
	// so pointers to it can be cached.
	struct ComponentTypeInfo
	{
		std::string Name;
		ComponentId Id = InvalidComponentId;
		uint32_t Size = 0;
		uint32_t Alignment = 0;

		// Move-construct into destination and destroy source. nullptr = memcpy is enough.
		void (*Relocate)(void* destination, void* source) = nullptr;
		// nullptr = trivially destructible.
		void (*Destruct)(void* object) = nullptr;
		// Constructs a default value into uninitialised memory. nullptr = not default-constructible.
		void (*DefaultConstruct)(void* destination) = nullptr;
		// Calls the type's Serialize(Archive&). nullptr = the type has none (not saved, not in the Inspector).
		void (*Serialize)(void* object, Archive& archive) = nullptr;

		// The module that registered it was unloaded (hot reload): the function pointers are
		// invalid. A stale type isn't found by name until the new module registers it again.
		bool bIsStale = false;

		void RelocateObject(void* destination, void* source) const
		{
			if (Relocate) {
				Relocate(destination, source);
			} else {
				std::memcpy(destination, source, Size);
			}
		}

		void DestroyObject(void* object) const
		{
			if (Destruct) {
				Destruct(object);
			}
		}
	};

	// Process-wide list of component types, shared by every module through FlexCore.
	// Thread-safe. Types are registered lazily on first use (see GetComponentType).
	class FLEX_API ComponentRegistery
	{
	public:
		// Registers a type, or returns the existing one with the same name.
		// A second registration with a different size or alignment is an error
		// (two different structs using the same component name), unless the existing one is
		// stale: then it's replaced (same id) with the new layout and functions.
		static const ComponentTypeInfo& Register(const ComponentTypeInfo& info);

		static const ComponentTypeInfo& GetInfo(ComponentId id);
		static const ComponentTypeInfo* FindByName(const std::string& name);
		static uint32_t GetCount();

		// Every registered type that isn't stale, in id order.
		static std::vector<const ComponentTypeInfo*> GetAll();

		// Marks every type matching the predicate stale (before its module is unloaded; no
		// component of those types may still exist). Returns how many were marked.
		static size_t MarkStale(const std::function<bool(const ComponentTypeInfo&)>& predicate);
	};

	template<IsComponent T>
	ComponentTypeInfo MakeComponentTypeInfo()
	{
		ComponentTypeInfo info;
		info.Name = T::ComponentName;
		info.Size = static_cast<uint32_t>(sizeof(T));
		info.Alignment = static_cast<uint32_t>(alignof(T));
		if constexpr (!std::is_trivially_copyable_v<T>) {
			info.Relocate = [](void* destination, void* source)
			{
				T* sourceObject = static_cast<T*>(source);
				std::construct_at(static_cast<T*>(destination), std::move(*sourceObject));
				std::destroy_at(sourceObject);
			};
		}
		if constexpr (!std::is_trivially_destructible_v<T>) {
			info.Destruct = [](void* object) { std::destroy_at(static_cast<T*>(object)); };
		}
		if constexpr (std::is_default_constructible_v<T>) {
			info.DefaultConstruct = [](void* destination) { ::new (destination) T(); };
		}
		if constexpr (requires(T& component, Archive& archive) { component.Serialize(archive); }) {
			info.Serialize = [](void* object, Archive& archive) { static_cast<T*>(object)->Serialize(archive); };
		}
		return info;
	}

	// Registered type of T. Cached per module after the first call.
	template<IsComponent T>
	const ComponentTypeInfo& GetComponentType()
	{
		static const ComponentTypeInfo& Info = ComponentRegistery::Register(MakeComponentTypeInfo<T>());
		return Info;
	}

	template<IsComponent T>
	ComponentId GetComponentId()
	{
		return GetComponentType<T>().Id;
	}

	// True when no type appears twice in Ts.
	template<typename... Ts>
	struct DistinctTypes : std::true_type {};

	template<typename T, typename... Rest>
	struct DistinctTypes<T, Rest...> : std::bool_constant<(!std::is_same_v<T, Rest> && ...) && DistinctTypes<Rest...>::value> {};
}
