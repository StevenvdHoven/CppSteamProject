#pragma once
#include <iostream>
#include "Component.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <fstream>

class Component;

using ComponentFactoryFn = std::unique_ptr<Component>(*)();

class ComponentRegistery
{
public:
    static void Register(const std::string& name, ComponentFactoryFn fn)
    {
        factories[name] = fn;
    }

    static std::unique_ptr<Component> Create(const std::string& name)
    {
        auto it = factories.find(name);

        if (it == factories.end())
            return nullptr;

        return it->second();
    }

    static std::vector<std::string> GetAvailableComponentTypes()
    {
        std::vector<std::string> keys;
        keys.reserve(factories.size());

        for (auto entry : factories)
        {
            keys.emplace_back(entry.first);
        }
        return keys;
    }

private:
    static std::unordered_map<std::string, ComponentFactoryFn> factories;
};

template<typename T>
class ComponentRegister
{
public:
    ComponentRegister(const std::string& name)
    {
        ComponentRegistery::Register(name, []() -> std::unique_ptr<Component>
            {
                return std::make_unique<T>();
            });
    }
};

#define DECLARE_COMPONENT(Type, Name) \
    static inline ComponentRegister<Type> component_registrar{Name}; \
	virtual const std::string GetName() const { return Name; }