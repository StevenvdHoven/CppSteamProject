#pragma once
#include "iostream"
#include "string"
#include "steam/steam_api.h"


int main() 
{
	SteamAPI_Init();

	std::string result;
	std::cin >> result;
	return 0;
}