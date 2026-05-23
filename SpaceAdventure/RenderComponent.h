#pragma once
#include "Component.h"
class RenderComponent : public Component
{
public:
	RenderComponent();

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

