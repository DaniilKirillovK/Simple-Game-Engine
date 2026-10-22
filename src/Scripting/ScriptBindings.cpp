// Python module "engine": the boundary between C++ and scripts.
//
// Ownership rules:
//  - scripts get engine.Entity, a weak handle (id only). Every access re-validates
//    the id, so a destroyed entity raises RuntimeError instead of touching freed memory;
//  - component values (Vec3) are returned by copy, never as references into pools;
//  - C++ keeps behaviour instances only as py::object in ScriptSystem and drops them on Stop.

#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <cstdint>
#include <functional>

#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptBehaviour.h"
#include "Systems/ScriptSystem.h"
#include "World.h"
#include "InputHandler.h"
#include "Logger.h"
#include "Components/Transform.h"
#include "Components/Tag.h"
#include "Components/Rigidbody.h"

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
}
