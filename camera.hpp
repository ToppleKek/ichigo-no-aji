#pragma once
#include "entity.hpp"

namespace Ichigo {
namespace Camera {
enum Mode {
    MANUAL,
    FOLLOW,
};

extern Vec2<f32> offset;
extern Mat4<f32> transform;
// TODO: This works by centering the camera on this point (in manual mode). Is this good?
extern Vec2<f32> manual_focus_point;
extern Mode mode;

void follow(Ichigo::EntityID entity);
void update();
}
}