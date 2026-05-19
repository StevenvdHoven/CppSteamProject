#include "AssetRegistery.h"

std::unordered_map<std::string, AssetFactoryFn> AssetRegistery::factories;
std::unordered_map<std::string, AssetFactoryFn> AssetRegistery::extentionFactories;