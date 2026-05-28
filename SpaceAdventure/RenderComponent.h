#pragma once
#include "Component.h"
class RenderComponent : public Component
{
public:
	RenderComponent();
	virtual ~RenderComponent();

	virtual void Load(std::ifstream& inputstream) override;
	virtual void Save(std::ofstream& outputstream) override;

	virtual void Render() {};

#ifdef _DEBUG
	virtual void ImGui_OnWindow() override;
#endif // _DEBUG

public:
	int Order;
};
