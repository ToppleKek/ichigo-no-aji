#pragma once

#include "../ichigo.hpp"

namespace Entrances {
void init();
void spawn_entrance(const Ichigo::EntityDescriptor &descriptor);
void spawn_entrance_trigger(const Ichigo::EntityDescriptor &descriptor);
void spawn_death_trigger(const Ichigo::EntityDescriptor &descriptor);
void spawn_locked_door(const Ichigo::EntityDescriptor &descriptor, u64 unlock_flags, Vec2<f32> exit_location, u8 unlocked_bit);
void spawn_key(const Ichigo::EntityDescriptor &descriptor);
void spawn_shop_entrance(const Ichigo::EntityDescriptor &descriptor);
Bana::Optional<Vec2<f32>> get_exit_location_if_possible(Ichigo::EntityID eid);
}
