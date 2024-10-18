#pragma once
#include "entity.hpp"

namespace Ichigo {
namespace Camera {
extern Vec2<f32> offset;
extern Mat4<f32> transform;

void follow(Ichigo::EntityID entity);
void update();
}
}