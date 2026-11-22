#pragma once
#include "ISystem.h"
#include "IRenderAdapter.h"
#include <glm/glm.hpp>

class RenderSystem : public ISystem
{
public:
	struct Statistics
	{
		size_t totalRenderers = 0;
		size_t candidates = 0;
		size_t boundsTested = 0;
		size_t culled = 0;
		size_t submitted = 0;
		float visibilityCpuMs = 0.0f;
		float submissionCpuMs = 0.0f;
	};

	RenderSystem(IRenderAdapter* renderAdapter);
	virtual void update(World& world, float deltaTime) override;
	virtual void setEnabled(bool isEnabled) override;
	void setFrustumCullingEnabled(bool enabled) { m_frustumCullingEnabled = enabled; }
	bool isFrustumCullingEnabled() const { return m_frustumCullingEnabled; }
	const Statistics& getStatistics() const { return m_statistics; }

private:
	bool m_isEnabled = true;
	IRenderAdapter* m_renderAdapter = nullptr;

	std::vector<Light*> m_lights;
	bool m_frustumCullingEnabled = true;
	Statistics m_statistics;
	std::vector<size_t> m_visibleRenderers;
};
