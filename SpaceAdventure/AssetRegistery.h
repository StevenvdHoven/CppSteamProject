#pragma once
#include <iostream>
#include <unordered_map>
#include "Asset.h"

using AssetFactoryFn = std::unique_ptr<Asset>(*)();

class AssetRegistery
{
    static void Register(const std::string& name, AssetFactoryFn fn)
    {
        factories[name] = fn;
    }

    static std::unique_ptr<Asset> Create(const std::string& name)
    {
        auto it = factories.find(name);

        if (it == factories.end())
            return nullptr;

        return it->second();
    }

private:
    static std::unordered_map<std::string, std::unique_ptr<Asset>(*)()> factories;
};

template<typename T>
class AssetRegister
{
public:
    AssetRegister(const std::string& name)
    {
        AssetRegistery::Register(name, []()
            {
                return std::make_unique<T>();
            });
    }
};

#define ASSET_LOOKUP(Type, Name) \
public: \
    static inline AssetRegister<Type> _asset_registrar{Name}; \
    static constexpr const char* AssetTypeName = Name;
