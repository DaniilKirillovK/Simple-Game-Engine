import engine


class EntityField:
    def __init__(self, name):
        self.name = name

    def __get__(self, instance, owner=None):
        if instance is None:
            return self

        references = instance.__dict__.get("_engine_entity_refs")

        if references is None:
            return None

        reference = references.get(self.name)

        if reference is None:
            return None

        entity_id, session_id = reference

        entity = engine.resolve_entity(entity_id, session_id)

        if entity is None:
            references.pop(self.name, None)

        return entity

    def __set__(self, instance, value):
        references = instance.__dict__.setdefault(
            "_engine_entity_refs",
            {},
        )

        if value is None:
            references.pop(self.name, None)
            return

        if not isinstance(value, engine.Entity):
            raise TypeError(
                f"Field '{self.name}' expects engine.Entity or None"
            )

        if not value.is_valid():
            references.pop(self.name, None)
            return

        references[self.name] = (
            value.id,
            engine.current_session_id(),
        )


def install_entity_field(behaviour_class, name):
    existing = getattr(behaviour_class, name, None)

    if isinstance(existing, EntityField):
        return

    if existing is not None:
        raise ValueError(
            f"Entity field '{name}' conflicts with a class attribute"
        )

    setattr(
        behaviour_class,
        name,
        EntityField(name),
    )