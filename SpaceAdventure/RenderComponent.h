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

	virtual void Render() const {};

public:
	int Order;
};

