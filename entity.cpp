#include "entity.hpp"
#include "util.hpp"

Util::IchigoVector<Ichigo::Entity> Ichigo::Internal::entities{512};

// TODO: If the entity id's index is 0, that means that entity has been killed and that spot can hold a new entity.
//       Should it really be like this? Maybe there should be an 'is_alive' flag on the entity?

Ichigo::Entity *Ichigo::spawn_entity() {
    // Search for an open entity slot
    for (u32 i = 1; i < Internal::entities.size; ++i) {
        if (Internal::entities.at(i).id.index == 0) {
            Ichigo::EntityID new_id = {Internal::entities.at(i).id.generation + 1, i};
            std::memset(&Internal::entities.at(i), 0, sizeof(Ichigo::Entity));
            Internal::entities.at(i).id = new_id;
            return &Internal::entities.at(i);
        }
    }

    // If no slots are available, append the entity to the end of the entity list
    Ichigo::Entity ret{};
    ret.id = {0, (u32) Internal::entities.size };
    Internal::entities.append(ret);
    return &Internal::entities.at(Internal::entities.size - 1);
}

Ichigo::Entity *Ichigo::get_entity(Ichigo::EntityID id) {
    if (id.index < 1 || id.index >= Internal::entities.size)
        return nullptr;

    if (Internal::entities.at(id.index).id.generation != id.generation)
        return nullptr;

    return &Internal::entities.at(id.index);
}

void Ichigo::kill_entity(EntityID id) {
    Ichigo::Entity *entity = Ichigo::get_entity(id);
    if (!entity) {
        ICHIGO_ERROR("Tried to kill a non-existant entity!");
        return;
    }

    entity->id.index = 0;
}