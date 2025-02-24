#pragma once

#include "../ichigo.hpp"

namespace MovingPlatform {
void init();
void spawn(const Ichigo::EntityDescriptor &descriptor);
void spawn_elevator(const Ichigo::EntityDescriptor &descriptor);
void deinit();
}
