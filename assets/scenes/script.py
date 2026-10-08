import json
from pathlib import Path

INPUT = Path("test_scene.json")
OUTPUT = Path("test_scene_250_green_balls.json")

# Сколько ВСЕГО зелёных шаров должно быть в итоговой сцене
TARGET_TOTAL_GREEN = 250

# Если понадобится добавить ровно N новых, не считая старых:
# TARGET_TOTAL_GREEN = None
# ADD_COUNT = 250

with INPUT.open("r", encoding="utf-8") as f:
    scene = json.load(f)

entities = scene["entities"]

existing_green = sum(
    1 for e in entities
    if e.get("tag", {}).get("name") == "Green Ball"
)

if TARGET_TOTAL_GREEN is not None:
    to_add = TARGET_TOTAL_GREEN - existing_green
    if to_add < 0:
        print(f"В сцене уже {existing_green} зелёных шаров — больше, чем {TARGET_TOTAL_GREEN}.")
        print("Чтобы сохранить предыдущую сцену, ничего не удаляю. Добавлено 0 шаров.")
        to_add = 0
else:
    to_add = ADD_COUNT

max_id = max((e.get("id", 0) for e in entities), default=0)


def make_green_ball(eid, pos):
    return {
        "collider": {
            "bounciness": 0.5,
            "friction": 0.4,
            "is_trigger": False,
            "offset": [0.0, 0.0, 0.0],
            "radius": 0.5,
            "type": "sphere"
        },
        "id": eid,
        "mesh_renderer": {
            "material": {
                "ambient_color": [0.2, 0.2, 0.2, 0.2],
                "diffuse_color": [0.2, 0.8, 0.2, 1.0],
                "fragment_shader_path": "Shaders\\Default.frag",
                "has_texture": False,
                "shininess": 32.0,
                "specular_color": [0.5, 0.5, 0.5, 0.5],
                "vertex_shader_path": "Shaders\\Default.vert"
            },
            "mesh": {
                "radius": 0.5,
                "type": "sphere"
            },
            "render_layer": 0,
            "visible": True
        },
        "rigidbody": {
            "is_kinematic": False,
            "mass": 1.0,
            "use_gravity": True,
            "velocity": [0.0, 0.0, 0.0]
        },
        "tag": {
            "name": "Green Ball"
        },
        "transform": {
            "position": pos,
            "rotation": [0.0, -0.0, 0.0],
            "scale": [1.0, 1.0, 1.0]
        }
    }


positions = []
if to_add > 0:
    cols = 15
    rows = 15
    per_layer = cols * rows
    layers = (to_add + per_layer - 1) // per_layer
    y_start = 12.0
    y_step = 2.0

    for idx in range(to_add):
        layer = idx // per_layer
        rem = idx % per_layer
        col = rem % cols
        row = rem // cols

        x = -9.0 + col * (18.0 / (cols - 1))
        z = -9.0 + row * (18.0 / (rows - 1))
        y = y_start + layer * y_step

        positions.append([round(x, 6), round(y, 6), round(z, 6)])

for i, pos in enumerate(positions, start=1):
    entities.append(make_green_ball(max_id + i, pos))

with OUTPUT.open("w", encoding="utf-8") as f:
    json.dump(scene, f, indent=4, ensure_ascii=False)

total_green = sum(
    1 for e in entities
    if e.get("tag", {}).get("name") == "Green Ball"
)

print(f"Исходная сцена сохранена без изменений: {INPUT}")
print(f"Результат записан в: {OUTPUT}")
print(f"Было зелёных шаров: {existing_green}")
print(f"Добавлено новых: {len(positions)}")
print(f"Всего зелёных шаров теперь: {total_green}")