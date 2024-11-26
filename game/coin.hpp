#pragma once
#include "../ichigo.hpp"

namespace Coin {
void init();
Ichigo::EntityID spawn(Vec2<f32> pos, Ichigo::EntityCollideProc collide_proc);
}