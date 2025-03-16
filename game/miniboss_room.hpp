#pragma once
#include "ichiaji.hpp"

#define MINIBOSS_COMPLETE_FLAG (1 << 3)

namespace Miniboss {
void spawn_controller_entity(const Ichigo::EntityDescriptor &descriptor);
}
