import engine


def player_behaviour(player):
    if player is None or not player.is_valid():
        return None
    return player.behaviour


def fit_collider(entity, margin=0.0):
    scale = entity.scale
    try:
        entity.collider_size = engine.Vec3(
            abs(scale.x) * 0.5 + margin,
            abs(scale.y) * 0.5 + margin,
            abs(scale.z) * 0.5 + margin,
        )
        entity.collider_offset = engine.Vec3(0.0, 0.0, 0.0)
        return True
    except RuntimeError:
        engine.log_warning(f"{entity.name}: add a Collider with is_trigger so it can detect the player")
        return False


def approach(current, target, step):
    if current < target:
        return min(current + step, target)
    return max(current - step, target)


def lerp_vec(a, b, t):
    return a + (b - a) * t
