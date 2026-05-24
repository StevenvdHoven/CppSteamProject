#pragma once
#include <iostream>
#include "Asset.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <fstream>

class Asset;

using AssetFactoryFn = std::unique_ptr<Asset>(*)();

class AssetRegistery
{
public:
    static void Register(const std::string& name, std::vector<std::string> extentions, AssetFactoryFn fn)
    {
        factories[name] = fn;
        
        for (auto extention : extentions)
        {
            extentionFactories[extention] = fn;
        }
    }

    static std::unique_ptr<Asset> Create(const std::string& name)
    {
        auto it = factories.find(name);
        
        if (it == factories.end())
            return nullptr;

        return it->second();
    }

    static std::unique_ptr<Asset> CreateFromExtention(const std::string& extention)
    {
        auto it = extentionFactories.find(extention);
        if (it == extentionFactories.end()) {
            return nullptr;
        }
        return it->second();
    }

private:
    static std::unordered_map<std::string, AssetFactoryFn> factories;
    static std::unordered_map<std::string, AssetFactoryFn> extentionFactories;
};

template<typename T>
class AssetRegister
{
public:
    AssetRegister(const std::string& name, std::vector<std::string> extentions)
    {
        AssetRegistery::Register(name, extentions, []() -> std::unique_ptr<Asset>
            {
                return std::make_unique<T>();
            });
    }
};

#define ASSET_LOOKUP(Type, Name, ...) \
public: \
    static inline AssetRegister<Type> _asset_registrar{Name, std::vector<std::string>({__VA_ARGS__})}; \
    static constexpr const char* AssetTypeName = Name; \
    virtual const std::vector<std::string> GetExtentions() override { return std::vector<std::string>({__VA_ARGS__}); } \
    virtual const std::string GetType() override { return Name; }
