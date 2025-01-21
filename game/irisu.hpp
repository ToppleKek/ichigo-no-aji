#pragma once
#include "../ichigo.hpp"

namespace Irisu {
void init();
void spawn(Ichigo::Entity *entity, Vec2<f32> pos);
void deinit();
void update(Ichigo::Entity *irisu);
}