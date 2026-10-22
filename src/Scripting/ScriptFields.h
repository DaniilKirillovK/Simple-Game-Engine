#pragma once

#include "Components/Script.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/EntityReferenceData.h"

#include <pybind11/pybind11.h>

#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ScriptFields
{
    namespace py = pybind11;
    using Json = nlohmann::json;

    enum class Type
    {
        Float,
        Int,
        Bool,
        String,
        Enum,
        Entity,
    };

    struct Field
    {
        std::string name;
        std::string label;
        Type type = Type::Float;

        Json defaultValue;

        std::optional<double> minimum;
        std::optional<double> maximum;

        std::vector<std::string> options;
    };

    inline Json pythonScalarToJson(py::handle value)
    {
        if (value.is_none())
        {
            return nullptr;
        }
        if (py::isinstance<py::bool_>(value))
        {
            return py::cast<bool>(value);
        }

        if (py::isinstance<py::int_>(value))
        {
            return py::cast<long long>(value);
        }

        if (py::isinstance<py::float_>(value))
        {
            return py::cast<double>(value);
        }

        if (py::isinstance<py::str>(value))
        {
            return py::cast<std::string>(value);
        }

        throw std::runtime_error("Editable defaults must be None, bool, int, float or str");
    }

    inline std::string validationError(
        const Field& field,
        const Json& value)
    {
        switch (field.type)
        {
        case Type::Float:
        {
            if (!value.is_number())
            {
                return "expected a number";
            }

            const double number = value.get<double>();

            if (!std::isfinite(number) ||
                std::abs(number) > std::numeric_limits<float>::max())
            {
                return "number is outside the float range";
            }

            break;
        }

        case Type::Int:
        {
            if (!value.is_number_integer())
            {
                return "expected an integer";
            }

            const double number = value.get<double>();

            if (number < std::numeric_limits<int>::lowest() ||
                number > std::numeric_limits<int>::max())
            {
                return "integer is outside the supported range";
            }

            break;
        }

        case Type::Bool:
        {
            if (!value.is_boolean())
            {
                return "expected bool";
            }
            break;
        }

        case Type::String:
        {
            if (!value.is_string())
            {
                return "expected string";
            }
            break;
        }

        case Type::Enum:
        {
            if (!value.is_string())
            {
                return "expected an enum string";
            }

            const std::string selected = value.get<std::string>();
            bool found = false;

            for (const std::string& option : field.options)
            {
                if (selected == option)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                return "value is absent from enum options";
            }

            break;
        }
        case Type::Entity:
        {
            if (!value.is_null() &&
                !EntityReferenceData::isReference(value))
            {
                return "expected null or an entity reference";
            }

            break;
        }
        }

        if (field.type == Type::Float || field.type == Type::Int)
        {
            const double number = value.get<double>();

            if (field.minimum && number < *field.minimum)
            {
                return "value is below min";
            }

            if (field.maximum && number > *field.maximum)
            {
                return "value is above max";
            }
        }

        return {};
    }

    inline std::vector<Field> readSchema(py::handle behaviourClass)
    {
        std::vector<Field> result;

        if (!py::hasattr(behaviourClass, "editor_fields"))
        {
            return result;
        }

        py::object declaration = behaviourClass.attr("editor_fields");

        if (!py::isinstance<py::dict>(declaration))
        {
            throw std::runtime_error("editor_fields must be a dict");
        }

        py::dict definitions = declaration.cast<py::dict>();

        for (auto item : definitions)
        {
            Field field;
            field.name = py::cast<std::string>(item.first);
            field.label = field.name;

            if (!py::isinstance<py::dict>(item.second))
            {
                throw std::runtime_error( "Field '" + field.name + "' must be a dict");
            }

            py::dict definition = py::reinterpret_borrow<py::dict>(item.second);

            if (!definition.contains("type") ||
                !definition.contains("default"))
            {
                throw std::runtime_error("Field '" + field.name + "' requires type and default");
            }

            const std::string type = py::cast<std::string>(definition["type"]);

            if (type == "float")
            {
                field.type = Type::Float;
            }
            else if (type == "int")
            {
                field.type = Type::Int;
            }
            else if (type == "bool")
            {
                field.type = Type::Bool;
            }
            else if (type == "str")
            {
                field.type = Type::String;
            }
            else if (type == "enum")
            {
                field.type = Type::Enum;
            }
            else if (type == "entity")
            {
                field.type = Type::Entity;
            }
            else
            {
                throw std::runtime_error("Unknown type '" + type + "' for field '" + field.name + "'");
            }

            if (definition.contains("label"))
            {
                field.label = py::cast<std::string>(definition["label"]);
            }

            field.defaultValue = pythonScalarToJson(definition["default"]);

            if (definition.contains("min"))
            {
                field.minimum = py::cast<double>(definition["min"]);
            }

            if (definition.contains("max"))
            {
                field.maximum = py::cast<double>(definition["max"]);
            }

            const bool numeric =
                field.type == Type::Float ||
                field.type == Type::Int;

            if (!numeric && (field.minimum || field.maximum))
            {
                throw std::runtime_error("Bounds require a numeric field: " + field.name);
            }

            for (const auto& bound : { field.minimum, field.maximum })
            {
                if (!bound)
                {
                    continue;
                }

                const double limit =
                    field.type == Type::Int
                    ? static_cast<double>(std::numeric_limits<int>::max())
                    : static_cast<double>(std::numeric_limits<float>::max());

                const double lowerLimit =
                    field.type == Type::Int
                    ? static_cast<double>(std::numeric_limits<int>::lowest())
                    : -limit;

                if (!std::isfinite(*bound) ||
                    *bound < lowerLimit ||
                    *bound > limit)
                {
                    throw std::runtime_error("Invalid bound for field: " + field.name);
                }

                if (field.type == Type::Int &&
                    std::floor(*bound) != *bound)
                {
                    throw std::runtime_error("Integer bounds must be integers: " + field.name);
                }
            }

            if (field.minimum && 
                field.maximum &&
                *field.minimum > *field.maximum)
            {
                throw std::runtime_error("min exceeds max for field: " + field.name);
            }

            if (field.type == Type::Enum)
            {
                if (!definition.contains("options"))
                {
                    throw std::runtime_error("Enum requires options: " + field.name);
                }

                py::object options = definition["options"];

                if (!py::isinstance<py::list>(options) &&
                    !py::isinstance<py::tuple>(options))
                {
                    throw std::runtime_error("Enum options must be a list or tuple: " +field.name);
                }

                for (py::handle option : py::reinterpret_borrow<py::iterable>(options))
                {
                    field.options.push_back(py::cast<std::string>(option));
                }

                if (field.options.empty())
                {
                    throw std::runtime_error( "Enum options are empty: " + field.name);
                }
            }

            const std::string error = validationError(field, field.defaultValue);

            if (!error.empty())
            {
                throw std::runtime_error("Invalid default for '" + field.name + "': " + error);
            }

            result.push_back(std::move(field));
        }

        return result;
    }

    inline Json effectiveValue(
        const Script& script,
        const Field& field)
    {
        if (script.fields.is_object() &&
            script.fields.contains(field.name))
        {
            return script.fields.at(field.name);
        }

        return field.defaultValue;
    }

    inline py::object toPython(
        const Field& field,
        const Json& value)
    {
        switch (field.type)
        {
            case Type::Float:
            {
                return py::float_(value.get<double>());
            }

            case Type::Int:
            {
                return py::int_(value.get<int>());
            }

            case Type::Bool:
            {
                return py::bool_(value.get<bool>());
            }

            case Type::String: 
            {
            }
            case Type::Enum:
            {
                return py::str(value.get<std::string>());
            }
            case Type::Entity:
            {
                const EntityId id = EntityReferenceData::read(value);

                if (id == INVALID_ENTITY)
                {
                    return py::none();
                }

                py::module_ engine = py::module_::import("engine");

                py::object sessionId = engine.attr("current_session_id")();

                return engine.attr("resolve_entity")(id, sessionId);
            }
        }

        throw std::runtime_error("Unsupported field type");
    }

    inline void apply(
        py::object& behaviour,
        py::handle behaviourClass,
        const Script& script)
    {
        const std::vector<Field> schema = readSchema(behaviourClass);

        for (const Field& field : schema)
        {
            const Json value = effectiveValue(script, field);

            const std::string error = validationError(field, value);

            if (!error.empty())
            {
                throw std::runtime_error
                (
                    "Invalid value for '" + field.name +
                    "': " + error
                );
            }

            if (field.type == Type::Entity)
            {
                py::module_ helpers = py::module_::import("engine_fields");

                helpers.attr("install_entity_field")
                    (
                    behaviourClass,
                    field.name
                    );
            }

            behaviour.attr(field.name.c_str()) = toPython(field, value);
        }
    }
}