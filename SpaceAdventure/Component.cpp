#include "Component.h"
#include "GameObject.h"

Transform* Component::GetTransform()
{
	return Owner->GetTransform();
}
