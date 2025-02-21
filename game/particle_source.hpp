#pragma once

#include "../ichigo.hpp"

namespace ParticleSource {
void init();
void spawn(const Ichigo::EntityDescriptor &descriptor, Ichigo::TextureID texture_id, f32 t_between_spawns, f32 t_particle, f32 total_t);
}
