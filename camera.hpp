#pragma once
#include "entity.hpp"

namespace Ichigo {
namespace Camera {
extern Vec2<f32> offset;

void follow(Ichigo::EntityID entity);
void update();
}
}