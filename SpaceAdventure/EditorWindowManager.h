#pragma once
#include "EditorWindow.h"
#include <iostream>
#include <vector>

class EditorWindowManager
{
public:
	EditorWindowManager() {}

	void Render();
public:
	template<typename T>
	T& Insert(T* editorWindow)
	{
		EditorWindows.emplace_back(std::unique_ptr<T>(editorWindow));
		return *editorWindow;
	}

private:
	using EditorWindowVector = std::vector<std::unique_ptr<EditorWindow>>;
	EditorWindowVector EditorWindows;
};

