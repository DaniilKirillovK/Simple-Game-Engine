#pragma once
#include "ISystem.h"
#include "IRenderAdapter.h"
#include <glm/glm.hpp>

class RenderSystem : public ISystem
{
public:
	RenderSystem(IRenderAdapter* renderAdapter);
	virtual void update(World& world, JobSystem* jobs, float deltaTime) override;
	virtual void setEnabled(bool isEnabled) override;

private:
	bool m_isEnabled = true;
	IRenderAdapter* m_renderAdapter = nullptr;

	std::vector<Light*> m_lights;
};