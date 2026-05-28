#ifdef _DEBUG
#pragma once
#include "EditorWindow.h"
#include <iostream>
#include <vector>

class EditorWindowManager
{
public:
	EditorWindowManager() 
	{
		if (Instance)
		{
			throw new std::exception("MULTIPLE EDITORWINDOWMANAGERS!!");
		}

		Instance = this;
	}

	void Render();
public:
	template<typename T>
	T& Insert(T* editorWindow)
	{
		EditorWindows.emplace_back(std::unique_ptr<T>(editorWindow));
		return *editorWindow;
	}

	template<typename T>
	T& GetEditorWindow()
	{
		for (const auto& editors : EditorWindows) {
			T* foundEditor = dynamic_cast<T*>(editors.get());
			if (foundEditor) {
				return *foundEditor;
			}
		}
		throw new std::exception("editor not found!");
	}

public:
	static EditorWindowManager& GetChecked() { return *Instance; }
	static EditorWindowManager* Get() { return Instance; }
private:
	using EditorWindowVector = std::vector<std::unique_ptr<EditorWindow>>;
	EditorWindowVector EditorWindows;

	static EditorWindowManager* Instance;
};
#endif
