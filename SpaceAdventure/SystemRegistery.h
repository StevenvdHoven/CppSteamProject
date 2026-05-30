#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

class System {

};

class SystemLocator
{
public:
    template<typename T>
    static void Register(std::unique_ptr<System>&& s)
    {
        auto& type = typeid(T);
        GetSystems().emplace(type.name(), std::move(s));
    }

    template<typename T>
    static T* GetSystem() {
        auto& type = typeid(T);
        if (GetSystems().contains(type.name())) {
            auto ptr = GetSystems()[std::string(type.name())].get();
            return (T*)(ptr);
        }

        return nullptr;
    }

private:
    static std::unordered_map<std::string, std::unique_ptr<System>>& GetSystems()
    {
        static std::unordered_map<std::string, std::unique_ptr<System>> factories;
        return factories;
    }
};

template<typename T>
class SystemRegister
{
public:
    SystemRegister()
    {
        auto temp = std::make_unique<T>();
        SystemLocator::Register<T>(std::move(temp));
    }
};

#define DECLARE_SYSTEM(Type) \
    static inline SystemRegister<Type> system_registrar{}; 