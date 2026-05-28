#pragma once

class GameEngine;

class Time
{
public:
	static float GetDeltaTime() { return DeltaTime; }


private:
	static float DeltaTime;

	friend GameEngine;
};

