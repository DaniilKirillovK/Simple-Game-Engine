#pragma once
#include "ISystem.h"
#include "IRenderAdapter.h"
#include <glm/glm.hpp>

class RenderSystem : public ISystem
{
public:
	RenderSystem(IRenderAdapter* renderAdapter);

	virtual void update(World& world, float deltaTime) override;
	void setProjectionMatrix(const glm::mat4& matrix) { projectionMatrix = matrix; }
	void setViewMatrix(const glm::mat4& matrix) { viewMatrix = matrix; }

private:
	IRenderAdapter* renderAdapter;
	glm::mat4 projectionMatrix;
	glm::mat4 viewMatrix;
};