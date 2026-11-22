#pragma once

#include <array>
#include <cmath>
#include <glm/glm.hpp>

class Frustum
{
public:
    explicit Frustum(const glm::mat4& viewProjection)
    {
        const glm::mat4 rows = glm::transpose(viewProjection);
        m_planes = { rows[3] + rows[0], rows[3] - rows[0],
                     rows[3] + rows[1], rows[3] - rows[1],
                     rows[3] + rows[2], rows[3] - rows[2] };

        for (glm::vec4& plane : m_planes)
        {
            const float length = glm::length(glm::vec3(plane));
            if (!finite(plane) || !std::isfinite(length) || length <= 0.0f)
                return;
            plane /= length;
            if (!finite(plane)) return;
        }
        m_valid = true;
    }

    bool intersects(const glm::vec3& min, const glm::vec3& max) const
    {
        if (!m_valid || !finite(glm::vec4(min, 0.0f)) || !finite(glm::vec4(max, 0.0f))
            || glm::any(glm::greaterThan(min, max)))
            return true;

        for (const glm::vec4& plane : m_planes)
        {
            const glm::vec3 positive(
                plane.x >= 0.0f ? max.x : min.x,
                plane.y >= 0.0f ? max.y : min.y,
                plane.z >= 0.0f ? max.z : min.z);
            const float distance = glm::dot(glm::vec3(plane), positive) + plane.w;
            if (distance < -1e-4f) return false;
        }
        return true;
    }

private:
    static bool finite(const glm::vec4& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y)
            && std::isfinite(value.z) && std::isfinite(value.w);
    }

    std::array<glm::vec4, 6> m_planes{};
    bool m_valid = false;
};
