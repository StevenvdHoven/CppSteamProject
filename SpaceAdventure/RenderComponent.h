#pragma once
#include "Component.h"
class RenderComponent : public Component
{
public:
	DECLARE_COMPONENT(RenderComponent, "Render Component")

	RenderComponent();
	virtual ~RenderComponent();

	bool operator>(const RenderComponent& other) {
		return Order > other.Order;
	}

	bool operator<(const RenderComponent& other) {
		return Order < other.Order;
	}

	virtual void Load(std::ifstream& inputstream) override;
	virtual void Save(std::ofstream& outputstream) override;

	virtual void Render() {};

#ifdef _DEBUG
	virtual void ImGui_OnWindow() override;
#endif // _DEBUG

public:
	int Order;
};

