#pragma once
#include "Component.h"
#include <string>

class Tag : public Component
{
public:
	Tag() {};
	Tag(std::string name) : name(name) {}

	std::string name;
};