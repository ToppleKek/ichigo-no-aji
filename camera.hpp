/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Camera functions and structures.

    Author:      Braeden Hong
    Last edited: 2024/11/30
*/

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
extern Vec2<f32> screen_tile_dimensions;
extern Mode mode;

void follow(Ichigo::EntityID entity);
void update();
}
}