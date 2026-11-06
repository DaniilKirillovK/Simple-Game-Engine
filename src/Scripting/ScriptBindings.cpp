#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <cstdint>
#include <functional>

#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptBehaviour.h"
#include "Scripting/ScriptFields.h"
#include "Scripting/EntityReferenceData.h"
#include "Systems/ScriptSystem.h"
#include "World.h"
#include "InputHandler.h"
#include "Logger.h"
#include "Components/Transform.h"
#include "Components/Tag.h"
#include "Components/Rigidbody.h"
#include "Components/Hierarchy.h"
#include "Components/Camera.h"
#include "Systems/CameraSystem.h"
#include "Utils/RayCast.h"
#include "Utils/EntityBounds.h"
#include "Components/Collider.h"
#include "tracy/Tracy.hpp"
#include "tracy/TracyC.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace py = pybind11;
using namespace pybind11::literals;

class PyScriptBehaviour : public ScriptBehaviour
{
public:
	using ScriptBehaviour::ScriptBehaviour;

	void onStart() override
	{
		PYBIND11_OVERRIDE_NAME(void, ScriptBehaviour, "on_start", onStart);
	}

	void onUpdate(float deltaTime) override
	{
		PYBIND11_OVERRIDE_NAME(void, ScriptBehaviour, "on_update", onUpdate, deltaTime);
	}

	void onDestroy() override
	{
		PYBIND11_OVERRIDE_NAME(void, ScriptBehaviour, "on_destroy", onDestroy);
	}
};

struct EntityHandle
{
	EntityId id = INVALID_ENTITY;
	std::uint64_t sessionId = 0;

	EntityHandle() = default;

	explicit EntityHandle(EntityId entityId) : id(entityId)
	{
		ScriptSystem* system = ScriptEngine::getInstance().getActiveSystem();

		if (system)
		{
			sessionId = system->getSessionId();
		}
	}
};

ScriptSystem& activeSystem()
{
	ScriptSystem* system = ScriptEngine::getInstance().getActiveSystem();
	if (!system) throw std::runtime_error("Engine API is available only in play mode");
	return *system;
}

World& activeWorld()
{
	return activeSystem().getWorld();
}

bool isValid(const EntityHandle& entity)
{
	ScriptSystem* system = ScriptEngine::getInstance().getActiveSystem();

	return system &&
		entity.sessionId == system->getSessionId() &&
		system->getWorld().isValidEntity(entity.id);
}

void requireValid(const EntityHandle& entity)
{
	if (!isValid(entity))
	{
		throw std::runtime_error
		(
			"Entity " + std::to_string(entity.id) +
			" is destroyed or belongs to an expired Play session"
		);
	}
}

template<typename T>
T& requireComponent(const EntityHandle& entity, const char* componentName)
{
	requireValid(entity);
	T* component = activeWorld().getComponent<T>(entity.id);
	if (!component)
	{
		throw std::runtime_error("Entity " + std::to_string(entity.id) + " has no " + componentName);
	}
	return *component;
}

std::string entityName(const EntityHandle& entity)
{
	requireValid(entity);
	Tag* tag = activeWorld().getComponent<Tag>(entity.id);
	return tag ? tag->name : std::string();
}

void setEntityName(const EntityHandle& entity, const std::string& name)
{
	requireValid(entity);
	World& world = activeWorld();
	if (Tag* tag = world.getComponent<Tag>(entity.id))
	{
		tag->name = name;
	}
	else
	{
		world.getComponentPool<Tag>().addComponent(entity.id, Tag{ name });
	}
}

std::vector<EntityHandle> findAll(const std::string& name)
{
	ZoneScopedN("engine.find_all");
	std::vector<EntityHandle> result;
	auto& tagPool = activeWorld().getComponentPool<Tag>();
	for (size_t i = 0; i < tagPool.size(); ++i)
	{
		if (tagPool.components()[i].name == name)
		{
			result.push_back(EntityHandle{ tagPool.entities()[i] });
		}
	}
	return result;
}

std::vector<EntityHandle> findInRadius(const glm::vec3& center, float radius, const std::string& name)
{
	ZoneScopedN("engine.find_in_radius");
	World& world = activeWorld();
	auto& transforms = world.getComponentPool<Transform>();

	std::vector<std::pair<float, EntityId>> hits;
	const float radiusSquared = radius * radius;

	for (size_t i = 0; i < transforms.size(); ++i)
	{
		const EntityId entity = transforms.entities()[i];

		if (!name.empty())
		{
			Tag* tag = world.getComponent<Tag>(entity);
			if (!tag || tag->name != name) continue;
		}

		const Transform& transform = transforms.components()[i];
		glm::vec3 position = transform.position;
		Hierarchy* hierarchy = world.getComponent<Hierarchy>(entity);
		if (hierarchy && hierarchy->parent != INVALID_ENTITY)
		{
			position = transform.getWorldPosition();
		}

		const glm::vec3 delta = position - center;
		const float distanceSquared = glm::dot(delta, delta);
		if (distanceSquared <= radiusSquared)
		{
			hits.emplace_back(distanceSquared, entity);
		}
	}

	std::sort(hits.begin(), hits.end());

	std::vector<EntityHandle> result;
	result.reserve(hits.size());
	for (const auto& [distanceSquared, entity] : hits)
	{
		result.push_back(EntityHandle{ entity });
	}
	return result;
}

struct ScriptRayHit
{
	EntityHandle entity;
	glm::vec3 point = glm::vec3(0.0f);
	float distance = 0.0f;
};

bool tagExcluded(World& world, EntityId entity, const std::string& name, const std::vector<std::string>& exclude)
{
	if (name.empty() && exclude.empty()) return false;

	Tag* tag = world.getComponent<Tag>(entity);
	if (!name.empty() && (!tag || tag->name != name)) return true;
	return tag && std::find(exclude.begin(), exclude.end(), tag->name) != exclude.end();
}

bool overlaps(const EntityHandle& a, const EntityHandle& b, float margin)
{
	ZoneScopedN("engine.overlaps");

	if (!isValid(a) || !isValid(b)) return false;

	World& world = activeWorld();
	Bounds boundsA;
	Bounds boundsB;
	if (!EntityBounds::compute(world, a.id, boundsA) || !EntityBounds::compute(world, b.id, boundsB)) return false;

	return EntityBounds::overlap(boundsA, boundsB, margin);
}

std::vector<EntityHandle> overlapBox(const glm::vec3& center, const glm::vec3& halfSize,
	const std::string& name, const std::vector<std::string>& exclude)
{
	ZoneScopedN("engine.overlap_box");

	World& world = activeWorld();
	const Bounds query{ center - glm::abs(halfSize), center + glm::abs(halfSize) };

	std::vector<EntityHandle> result;
	auto& transforms = world.getComponentPool<Transform>();
	for (size_t i = 0; i < transforms.size(); ++i)
	{
		const EntityId entity = transforms.entities()[i];
		if (tagExcluded(world, entity, name, exclude)) continue;

		Bounds bounds;
		if (EntityBounds::compute(world, entity, bounds) && EntityBounds::overlap(query, bounds))
		{
			result.push_back(EntityHandle{ entity });
		}
	}
	return result;
}

py::object raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
	const std::string& name, const py::object& ignore, const std::vector<std::string>& exclude)
{
	ZoneScopedN("engine.raycast");

	const float length = glm::length(direction);
	if (!(length > 0.0f) || !std::isfinite(length))
	{
		throw py::value_error("direction must be a non-zero vector");
	}
	if (!(maxDistance > 0.0f))
	{
		throw py::value_error("max_distance must be positive");
	}

	EntityId ignored = INVALID_ENTITY;
	if (!ignore.is_none())
	{
		if (!py::isinstance<EntityHandle>(ignore))
		{
			throw py::type_error("ignore must be engine.Entity or None");
		}
		ignored = ignore.cast<const EntityHandle&>().id;
	}

	World& world = activeWorld();

	auto accept = [&](EntityId entity)
	{
		return entity != ignored && !tagExcluded(world, entity, name, exclude);
	};

	Ray ray;
	ray.origin = origin;
	ray.direction = direction / length;

	RayHit hit;
	if (!RayCast::pickMesh(world, ray, hit, maxDistance, accept))
	{
		return py::none();
	}

	return py::cast(ScriptRayHit{ EntityHandle{ hit.entity }, hit.point, hit.distance });
}

nlohmann::json fieldsToJson(const py::object& fields)
{
	nlohmann::json result = nlohmann::json::object();
	if (fields.is_none()) return result;

	if (!py::isinstance<py::dict>(fields))
	{
		throw py::type_error("fields must be a dict or None");
	}

	for (auto item : fields.cast<py::dict>())
	{
		const std::string key = py::cast<std::string>(py::str(item.first));

		if (py::isinstance<EntityHandle>(item.second))
		{
			const EntityHandle& entity = item.second.cast<const EntityHandle&>();
			result[key] = isValid(entity) ? EntityReferenceData::make(entity.id) : nlohmann::json(nullptr);
			continue;
		}

		try
		{
			result[key] = ScriptFields::pythonScalarToJson(item.second);
		}
		catch (const std::exception& e)
		{
			throw py::type_error("Field '" + key + "': " + e.what() + " or engine.Entity");
		}
	}
	return result;
}

class ScriptZone
{
public:
	ScriptZone(std::string name, std::uint32_t color, std::string file, std::uint32_t line, std::string function)
		: m_name(std::move(name)), m_file(std::move(file)), m_function(std::move(function)), m_color(color), m_line(line)
	{
	}

	ScriptZone(const ScriptZone&) = delete;
	ScriptZone& operator=(const ScriptZone&) = delete;
	ScriptZone(ScriptZone&& other) noexcept
		: m_name(std::move(other.m_name)), m_file(std::move(other.m_file)), m_function(std::move(other.m_function)),
		m_color(other.m_color), m_line(other.m_line)
	{
	}

	~ScriptZone()
	{
		end();
	}

	ScriptZone& begin()
	{
#ifdef TRACY_ENABLE
		if (!m_active)
		{
			const std::uint64_t location = ___tracy_alloc_srcloc_name(m_line,
				m_file.c_str(), m_file.size(), m_function.c_str(), m_function.size(),
				m_name.c_str(), m_name.size(), m_color);
			m_context = ___tracy_emit_zone_begin_alloc(location, 1);
			m_active = true;
		}
#endif
		return *this;
	}

	void end()
	{
#ifdef TRACY_ENABLE
		if (m_active)
		{
			___tracy_emit_zone_end(m_context);
			m_active = false;
		}
#endif
	}

	void text(const std::string& value)
	{
#ifdef TRACY_ENABLE
		if (m_active)
		{
			___tracy_emit_zone_text(m_context, value.c_str(), value.size());
		}
#endif
	}

	void setValue(std::int64_t value)
	{
#ifdef TRACY_ENABLE
		if (m_active)
		{
			___tracy_emit_zone_value(m_context, static_cast<std::uint64_t>(value));
		}
#endif
	}

	bool isActive() const { return m_active; }

private:
	std::string m_name;
	std::string m_file;
	std::string m_function;
	std::uint32_t m_color = 0;
	std::uint32_t m_line = 0;
	bool m_active = false;
#ifdef TRACY_ENABLE
	TracyCZoneCtx m_context{};
#endif
};

ScriptZone zoneAtCaller(const std::string& name, std::uint32_t color)
{
	std::string file = "<python>";
	std::string function = "<module>";
	std::uint32_t line = 0;

	if (PyFrameObject* frame = PyEval_GetFrame())
	{
		line = static_cast<std::uint32_t>(PyFrame_GetLineNumber(frame));
		py::object code = py::reinterpret_steal<py::object>(reinterpret_cast<PyObject*>(PyFrame_GetCode(frame)));
		file = py::str(code.attr("co_filename"));
		function = py::str(code.attr("co_name"));
	}

	return ScriptZone(name, color, file, line, function);
}

void plotValue(const std::string& name, double value)
{
	static std::unordered_set<std::string> names;
	const std::string& stored = *names.insert(name).first;
	TracyPlot(stored.c_str(), value);
	(void)stored;
	(void)value;
}

PYBIND11_EMBEDDED_MODULE(engine, m)
{
	m.doc() = "Simple Game Engine scripting API";

	py::class_<glm::vec3>(m, "Vec3")
		.def(py::init<>())
		.def(py::init<float, float, float>(), "x"_a, "y"_a, "z"_a)
		.def_readwrite("x", &glm::vec3::x)
		.def_readwrite("y", &glm::vec3::y)
		.def_readwrite("z", &glm::vec3::z)
		.def("__add__", [](const glm::vec3& a, const glm::vec3& b) { return a + b; })
		.def("__sub__", [](const glm::vec3& a, const glm::vec3& b) { return a - b; })
		.def("__mul__", [](const glm::vec3& a, float s) { return a * s; })
		.def("__rmul__", [](const glm::vec3& a, float s) { return a * s; })
		.def("__neg__", [](const glm::vec3& a) { return -a; })
		.def("length", [](const glm::vec3& a) { return glm::length(a); })
		.def("normalized", [](const glm::vec3& a)
			{
				float length = glm::length(a);
				return length > 0.0f ? a / length : glm::vec3(0.0f);
			})
		.def("__repr__", [](const glm::vec3& a)
			{
				return "Vec3(" + std::to_string(a.x) + ", " + std::to_string(a.y) + ", " + std::to_string(a.z) + ")";
			});

	py::class_<EntityHandle>(m, "Entity")
		.def_property_readonly("id", [](const EntityHandle& e) { return e.id; })
		.def("is_valid", &isValid)
		.def_property("name", &entityName, &setEntityName)
		.def_property("position",
			[](const EntityHandle& e) { return requireComponent<Transform>(e, "Transform").position; },
			[](const EntityHandle& e, const glm::vec3& value)
			{
				Transform& transform = requireComponent<Transform>(e, "Transform");
				transform.position = value;
				transform.markDirty();
			})
		.def_property("rotation",
			[](const EntityHandle& e) { return requireComponent<Transform>(e, "Transform").eulerRotation; },
			[](const EntityHandle& e, const glm::vec3& degrees)
			{
				Transform& transform = requireComponent<Transform>(e, "Transform");
				transform.eulerRotation = degrees;
				transform.updateQuaternion();
				transform.markDirty();
			})
		.def_property("scale",
			[](const EntityHandle& e) { return requireComponent<Transform>(e, "Transform").scale; },
			[](const EntityHandle& e, const glm::vec3& value)
			{
				Transform& transform = requireComponent<Transform>(e, "Transform");
				transform.scale = value;
				transform.markDirty();
			})
		.def_property("velocity",
			[](const EntityHandle& e) { return requireComponent<Rigidbody>(e, "Rigidbody").velocity; },
			[](const EntityHandle& e, const glm::vec3& value) { requireComponent<Rigidbody>(e, "Rigidbody").velocity = value; })
		.def_property("collider_size",
			[](const EntityHandle& e) { return requireComponent<Collider>(e, "Collider").halfSize; },
			[](const EntityHandle& e, const glm::vec3& value) { requireComponent<Collider>(e, "Collider").halfSize = glm::abs(value); })
		.def_property("collider_offset",
			[](const EntityHandle& e) { return requireComponent<Collider>(e, "Collider").offset; },
			[](const EntityHandle& e, const glm::vec3& value) { requireComponent<Collider>(e, "Collider").offset = value; })
		.def_property("forward",
			[](const EntityHandle& e) { return requireComponent<Camera>(e, "Camera").forward; },
			[](const EntityHandle& e, const glm::vec3& value)
			{
				Camera& camera = requireComponent<Camera>(e, "Camera");
				const float length = glm::length(value);
				if (!(length > 0.0f) || !std::isfinite(length))
				{
					throw py::value_error("forward must be a non-zero vector");
				}

				camera.forward = value / length;
				glm::vec3 right = glm::cross(camera.forward, glm::vec3(0.0f, 1.0f, 0.0f));
				if (glm::length(right) < 1e-4f)
				{
					right = camera.right;
				}
				camera.right = glm::normalize(right);
				camera.up = glm::normalize(glm::cross(camera.right, camera.forward));
			})
		.def_property_readonly("behaviour",
			[](const EntityHandle& e) -> py::object
			{
				if (!isValid(e)) return py::none();
				return activeSystem().getBehaviour(e.id);
			})
		.def("destroy",
			[](const EntityHandle& entity)
			{
				requireValid(entity);
				activeSystem().requestDestroy(entity.id);
			})
		.def("__eq__",
			[](const EntityHandle& a, const EntityHandle& b)
			{
				return a.id == b.id &&
					a.sessionId == b.sessionId;
			})
		.def("__hash__",
			[](const EntityHandle& entity)
			{
				const std::size_t idHash = std::hash<EntityId>{}(entity.id);

				const std::size_t sessionHash = std::hash<std::uint64_t>{}(entity.sessionId);

				return idHash ^
					(sessionHash + static_cast<std::size_t>(0x9e3779b9) +
						(idHash << 6) + (idHash >> 2));
			})
		.def("__repr__", [](const EntityHandle& e) { return "Entity(" + std::to_string(e.id) + ")"; });

	py::class_<ScriptBehaviour, PyScriptBehaviour>(m, "Behaviour")
		.def(py::init<>())
		.def("on_start", &ScriptBehaviour::onStart)
		.def("on_update", &ScriptBehaviour::onUpdate, "dt"_a)
		.def("on_destroy", &ScriptBehaviour::onDestroy)
		.def_property_readonly("entity", [](const ScriptBehaviour& b) { return EntityHandle{ b.entityId }; });

	m.def("spawn", [](const std::string& primitive, const glm::vec3& position)
		{
			EntityId entity = activeSystem().spawn(primitive, position);
			if (entity == INVALID_ENTITY)
			{
				throw std::runtime_error("Cannot spawn '" + primitive + "', expected 'cube' or 'sphere'");
			}
			return EntityHandle{ entity };
		}, "primitive"_a, "position"_a = glm::vec3(0.0f));

	m.def("spawn_prefab", [](const std::string& name, const glm::vec3& position, const py::object& fields)
		{
			const nlohmann::json overrides = fieldsToJson(fields);
			EntityId entity = activeSystem().spawnPrefab(name, position, overrides);
			if (entity == INVALID_ENTITY)
			{
				throw std::runtime_error("Cannot spawn prefab '" + name + "', see the engine log");
			}
			return EntityHandle{ entity };
		}, "name"_a, "position"_a = glm::vec3(0.0f), "fields"_a = py::none(),
		"Spawns assets/prefabs/<name>.prefab.json. fields override the template's script fields.");

	m.def(
		"destroy",
		[](const EntityHandle& entity)
		{
			requireValid(entity);
			activeSystem().requestDestroy(entity.id);
		},
		"entity"_a
	);

	m.def("find", [](const std::string& name) -> py::object
		{
			std::vector<EntityHandle> found = findAll(name);
			if (found.empty()) return py::none();
			return py::cast(found.front());
		}, "name"_a);

	m.def("find_all", &findAll, "name"_a);

	m.def("find_in_radius", &findInRadius, "center"_a, "radius"_a, "name"_a = "",
		"Entities within radius of center, nearest first; name filters by tag");

	m.def("overlaps", &overlaps, "a"_a, "b"_a, "margin"_a = 0.0f,
		"True when the boxes of two entities intersect (Collider if present, otherwise mesh bounds); margin widens the test");
	m.def("overlap_box", &overlapBox, "center"_a, "half_size"_a, "name"_a = "", "exclude"_a = std::vector<std::string>{},
		"Entities whose boxes intersect the given box; name keeps only that tag, exclude skips tags");

	py::class_<ScriptRayHit>(m, "RaycastHit")
		.def_readonly("entity", &ScriptRayHit::entity)
		.def_readonly("point", &ScriptRayHit::point)
		.def_readonly("distance", &ScriptRayHit::distance)
		.def("__repr__", [](const ScriptRayHit& hit)
			{
				return "RaycastHit(entity=" + std::to_string(hit.entity.id) +
					", distance=" + std::to_string(hit.distance) + ")";
			});

	m.def("raycast", &raycast, "origin"_a, "direction"_a,
		"max_distance"_a = std::numeric_limits<float>::max(), "name"_a = "", "ignore"_a = py::none(),
		"exclude"_a = std::vector<std::string>{},
		"Nearest visible mesh hit by the ray or None; name keeps only that tag, ignore skips one entity, "
		"exclude skips every entity whose tag is in the list");

	m.def("hud", [](const std::string& key, const std::string& text) { activeSystem().setHudLine(key, text); },
		"key"_a, "text"_a, "Shows a HUD line over the viewport until Stop; lines are ordered by key");
	m.def("hud_clear", [](const std::string& key) { activeSystem().clearHud(key); }, "key"_a = "");
	m.def("screen_flash", [](const glm::vec3& color, float alpha, float duration)
		{
			activeSystem().setScreenFlash(color, std::clamp(alpha, 0.0f, 1.0f), std::max(0.0f, duration));
		}, "color"_a, "alpha"_a = 0.6f, "duration"_a = 0.4f,
		"Tints the viewport edges with color; fades out over duration seconds, 0 keeps it until replaced, alpha 0 clears it");

	m.def("hud_center", [](const std::string& text) { activeSystem().setHudCenter(text); }, "text"_a = "",
		"Large message in the middle of the viewport until replaced; empty text hides it");

	m.def(
		"current_session_id",
		[]()
		{
			return activeSystem().getSessionId();
		}
	);

	m.def(
		"resolve_entity",
		[](EntityId id, std::uint64_t sessionId) -> py::object
		{
			EntityHandle handle{ id };

			if (handle.sessionId != sessionId || !isValid(handle))
			{
				return py::none();
			}

			return py::cast(handle);
		},
		"id"_a,
		"session_id"_a
	);

	py::enum_<KeyCode> keys(m, "Key");
	keys.value("SPACE", KeyCode::KEY_SPACE)
		.value("ENTER", KeyCode::KEY_ENTER)
		.value("ESCAPE", KeyCode::KEY_ESCAPE)
		.value("TAB", KeyCode::KEY_TAB)
		.value("LEFT_SHIFT", KeyCode::KEY_LEFT_SHIFT)
		.value("LEFT_CONTROL", KeyCode::KEY_LEFT_CONTROL)
		.value("UP", KeyCode::KEY_UP)
		.value("DOWN", KeyCode::KEY_DOWN)
		.value("LEFT", KeyCode::KEY_LEFT)
		.value("RIGHT", KeyCode::KEY_RIGHT);
	for (int k = static_cast<int>(KeyCode::KEY_A); k <= static_cast<int>(KeyCode::KEY_Z); ++k)
	{
		keys.value(std::string(1, static_cast<char>(k)).c_str(), static_cast<KeyCode>(k));
	}
	for (int k = static_cast<int>(KeyCode::KEY_0); k <= static_cast<int>(KeyCode::KEY_9); ++k)
	{
		keys.value(("NUM_" + std::string(1, static_cast<char>(k))).c_str(), static_cast<KeyCode>(k));
	}

	m.def("is_key_down", [](KeyCode key) { return InputHandler::getInstance().isKeyPressed(key); }, "key"_a);
	m.def("is_key_pressed", [](KeyCode key) { return InputHandler::getInstance().isKeyJustPressed(key); }, "key"_a);
	m.def("is_key_released", [](KeyCode key) { return InputHandler::getInstance().isKeyJustReleased(key); }, "key"_a);
	m.def("is_action_active", [](const std::string& action) { return InputHandler::getInstance().isActionActive(action); }, "action"_a);
	m.def("is_action_pressed", [](const std::string& action) { return InputHandler::getInstance().isActionJustPressed(action); }, "action"_a);
	m.def("is_mouse_down", [](int button) { return InputHandler::getInstance().isMouseButtonPressed(button); }, "button"_a);
	m.def("is_mouse_pressed", [](int button) { return InputHandler::getInstance().isMouseButtonJustPressed(button); }, "button"_a);

	m.def("lock_cursor", [](bool locked)
		{
			activeSystem();
			InputHandler::getInstance().setCursorLocked(locked);
		}, "locked"_a = true,
		"Hides the cursor and keeps mouse_delta unbounded while Play runs; Esc stops Play");
	m.def("is_cursor_locked", []() { return InputHandler::getInstance().isCursorLocked(); });

	m.def("camera", []() -> py::object
		{
			auto& cameras = activeWorld().getComponentPool<Camera>();
			for (size_t i = 0; i < cameras.size(); ++i)
			{
				if (cameras.components()[i].isActive)
				{
					return py::cast(EntityHandle{ cameras.entities()[i] });
				}
			}
			return py::none();
		}, "Entity of the active camera or None");

	m.def("set_free_camera", [](bool enabled)
		{
			if (CameraSystem* cameraSystem = activeWorld().getSystem<CameraSystem>())
			{
				cameraSystem->setEnabled(enabled);
			}
		}, "enabled"_a, "Editor fly camera (WASD, right mouse); restored on Stop");

	m.def("restart", []() { activeSystem().requestRestart(); },
		"Restores the scene saved on Play and starts all scripts again, at the start of the next frame");
	m.def("mouse_position", []()
		{
			InputHandler& input = InputHandler::getInstance();
			return py::make_tuple(input.getMouseX(), input.getMouseY());
		});
	m.def("mouse_delta", []()
		{
			InputHandler& input = InputHandler::getInstance();
			return py::make_tuple(input.getMouseDeltaX(), input.getMouseDeltaY());
		});

	m.def("log", [](const std::string& message) { LOG_INFO("[Script] " + message); }, "message"_a);
	m.def("log_warning", [](const std::string& message) { LOG_WARNING("[Script] " + message); }, "message"_a);
	m.def("log_error", [](const std::string& message) { LOG_ERROR("[Script] " + message); }, "message"_a);

	py::class_<ScriptZone>(m, "Zone")
		.def("__enter__", &ScriptZone::begin, py::return_value_policy::reference_internal)
		.def("__exit__", [](ScriptZone& zone, const py::object&, const py::object&, const py::object&)
			{
				zone.end();
				return false;
			})
		.def("text", &ScriptZone::text, "text"_a, "Text shown on the zone in Tracy")
		.def("value", &ScriptZone::setValue, "value"_a, "Number shown on the zone in Tracy")
		.def_property_readonly("active", &ScriptZone::isActive);

	m.def("zone", &zoneAtCaller, "name"_a, "color"_a = 0u,
		"Tracy zone for a with-block: with engine.zone(\"collisions\"): ... The source location is the calling line");
	m.def("_zone_at", [](const std::string& name, std::uint32_t color,
		const std::string& file, std::uint32_t line, const std::string& function)
		{
			return ScriptZone(name, color, file, line, function);
		}, "name"_a, "color"_a, "file"_a, "line"_a, "function"_a);
	m.def("plot", &plotValue, "name"_a, "value"_a, "Adds a point to the Tracy plot with this name");

	py::exec(R"(
import functools as _functools


def profile(function=None, *, name=None, color=0):
    def decorate(target):
        zone_name = name or target.__qualname__
        code = target.__code__
        location = (code.co_filename, code.co_firstlineno, target.__name__)

        @_functools.wraps(target)
        def wrapper(*args, **kwargs):
            with _zone_at(zone_name, color, *location):
                return target(*args, **kwargs)

        return wrapper

    if function is not None:
        return decorate(function)
    return decorate
)", m.attr("__dict__"));
}
