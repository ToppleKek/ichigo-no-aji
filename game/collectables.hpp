#pragma once

#include "../ichigo.hpp"

namespace Collectables {
void spawn_coin(Vec2<f32> pos);
void spawn_recovery_heart(Vec2<f32> pos);
void spawn_powerup(const Ichigo::EntityDescriptor &descriptor);
}