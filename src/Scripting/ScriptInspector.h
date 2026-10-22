#pragma once

#include "Components/Script.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptFields.h"
#include "World.h"
#include "Components/Tag.h"
#include "Scripting/EntityReferenceData.h"

#include "libs/imgui/imgui.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace ScriptInspector
{
    inline std::string entityLabel(
        World& world,
        EntityId id)
    {
        std::string name = "Entity";

        if (Tag* tag = world.getComponent<Tag>(id))
        {
            if (!tag->name.empty())
            {
                name = tag->name;
            }
        }

        return name + " (id: " + std::to_string(id) + ")";
    }

    inline bool drawEntityField(
        const ScriptFields::Field& field,
        ScriptFields::Json& value,
        World& world)
    {
        const EntityId selectedId = EntityReferenceData::read(value);

        std::string preview = "None";

        if (selectedId != INVALID_ENTITY)
        {
            if (world.isValidEntity(selectedId))
            {
                preview = entityLabel(world, selectedId);
            }
            else
            {
                preview =
                    "Missing (id: " +
                    std::to_string(selectedId) + ")";
            }
        }

        bool changed = false;

        if (ImGui::BeginCombo(
            field.label.c_str(),
            preview.c_str()))
        {
            if (ImGui::Selectable(
                "None",
                selectedId == INVALID_ENTITY))
            {
                value = nullptr;
                changed = true;
            }

            const auto& entities = world.getEntities();

            std::vector<EntityId> ids(
                entities.begin(),
                entities.end()
            );

            std::sort(ids.begin(), ids.end());

            for (EntityId id : ids)
            {
                const std::string idText = std::to_string(id);
                const std::string label = entityLabel(world, id);
                const bool isSelected = id == selectedId;

                ImGui::PushID(idText.c_str());

                if (ImGui::Selectable(
                    label.c_str(),
                    isSelected))
                {
                    value = EntityReferenceData::make(id);
                    changed = true;
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        return changed;
    }

    inline bool drawField(
        const ScriptFields::Field& field,
        ScriptFields::Json& value,
        World& world)
    {
        using ScriptFields::Type;

        const char* label = field.label.c_str();

        switch (field.type)
        {
        case Type::Float:
        {
            float number = value.get<float>();
            bool changed = false;

            if (field.minimum && field.maximum)
            {
                changed = ImGui::SliderFloat
                (
                    label,
                    &number,
                    static_cast<float>(*field.minimum),
                    static_cast<float>(*field.maximum)
                );
            }
            else
            {
                changed = ImGui::DragFloat
                (
                    label,
                    &number,
                    0.1f
                );
            }

            if (changed)
            {
                if (field.minimum)
                {
                    number = std::max
                    (
                        number,
                        static_cast<float>(*field.minimum)
                    );
                }

                if (field.maximum)
                {
                    number = std::min
                    (
                        number,
                        static_cast<float>(*field.maximum)
                    );
                }

                value = number;
            }

            return changed;
        }

        case Type::Int:
        {
            int number = value.get<int>();
            bool changed = false;

            if (field.minimum && field.maximum)
            {
                changed = ImGui::SliderInt
                (
                    label,
                    &number,
                    static_cast<int>(*field.minimum),
                    static_cast<int>(*field.maximum)
                );
            }
            else
            {
                changed = ImGui::InputInt(label, &number);
            }

            if (changed)
            {
                if (field.minimum)
                {
                    number = std::max
                    (
                        number,
                        static_cast<int>(*field.minimum)
                    );
                }

                if (field.maximum)
                {
                    number = std::min
                    (
                        number,
                        static_cast<int>(*field.maximum)
                    );
                }

                value = number;
            }

            return changed;
        }

        case Type::Bool:
        {
            bool enabled = value.get<bool>();

            if (ImGui::Checkbox(label, &enabled))
            {
                value = enabled;
                return true;
            }

            return false;
        }

        case Type::String:
        {
            const std::string text = value.get<std::string>();

            std::vector<char> buffer(text.size() + 1024, '\0');
            std::memcpy(buffer.data(), text.data(), text.size());

            if (ImGui::InputText(
                label,
                buffer.data(),
                buffer.size()))
            {
                value = std::string(buffer.data());
                return true;
            }

            return false;
        }

        case Type::Enum:
        {
            const std::string selected = value.get<std::string>();

            bool changed = false;

            if (ImGui::BeginCombo(label, selected.c_str()))
            {
                for (const std::string& option : field.options)
                {
                    const bool isSelected = option == selected;

                    if (ImGui::Selectable(
                        option.c_str(),
                        isSelected))
                    {
                        value = option;
                        changed = true;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }

            return changed;
        }

        case Type::Entity:
        {
            return drawEntityField(field, value, world);
        }
        }

        return false;
    }

    inline void draw(Script& script, World& world)
    {
        if (!ImGui::CollapsingHeader(
            "Script",
            ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        ImGui::Text("Module: %s", script.moduleName.c_str());
        ImGui::Text("Class: %s", script.className.c_str());

        ScriptEngine& engine = ScriptEngine::getInstance();

        if (!engine.isInitialized())
        {
            ImGui::TextWrapped("Python is not initialized.");
            return;
        }

        try
        {
            pybind11::object behaviourClass =
                engine.loadBehaviourClass
                (
                    script.moduleName,
                    script.className
                );

            if (behaviourClass.is_none())
            {
                ImGui::TextWrapped("Cannot load behaviour. Check the engine log.");
                return;
            }

            const std::vector<ScriptFields::Field> schema = ScriptFields::readSchema(behaviourClass);

            if (schema.empty())
            {
                ImGui::TextWrapped("This behaviour declares no editable fields.");
                return;
            }

            if (!script.fields.is_object())
            {
                ImGui::TextWrapped("Stored fields are invalid.");

                if (ImGui::Button("Reset all fields"))
                {
                    script.fields = ScriptFields::Json::object();
                }

                return;
            }

            for (const ScriptFields::Field& field : schema)
            {
                ImGui::PushID(field.name.c_str());

                ScriptFields::Json value = ScriptFields::effectiveValue(script, field);

                const std::string error = ScriptFields::validationError(field, value);

                if (!error.empty())
                {
                    ImGui::TextWrapped
                    (
                        "%s: %s",
                        field.label.c_str(),
                        error.c_str()
                    );

                    if (ImGui::Button("Reset to default"))
                    {
                        script.fields[field.name] = field.defaultValue;
                    }

                    ImGui::PopID();
                    continue;
                }

                const bool changed = drawField(field, value, world);

                if (changed)
                {
                    const std::string editedError = ScriptFields::validationError(field, value);

                    if (editedError.empty())
                    {
                        script.fields[field.name] = value;
                    }
                    else
                    {
                        ImGui::TextWrapped
                        (
                            "%s",
                            editedError.c_str()
                        );
                    }
                }

                ImGui::SameLine();

                if (ImGui::SmallButton("Reset"))
                {
                    script.fields[field.name] = field.defaultValue;
                }

                ImGui::PopID();
            }
        }
        catch (const std::exception& e)
        {
            ImGui::TextWrapped
            (
                "Script fields error:\n%s",
                e.what()
            );
        }
    }
}