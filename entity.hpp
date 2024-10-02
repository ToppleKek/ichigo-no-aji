#pragma once
#include "common.hpp"
#include "math.hpp"
#include "texture.hpp"
#include "util.hpp"

namespace Ichigo {
struct Entity;
using EntityRenderProc = void (Entity *);
using EntityUpdateProc = void (Entity *);
using EntityFlags      = u64;

struct EntityID {
    u32 generation;
    u32 index;
};

enum EntityFlag {
    EF_ON_GROUND = 1 << 0
};

struct Entity {
    EntityID id;
    Rectangle col;
    Vec2<f32> velocity;
    Vec2<f32> acceleration;
    Vec2<f32> max_velocity;
    Vec2<f32> sprite_pos_offset;
    f32 sprite_w;
    f32 sprite_h;
    f32 movement_speed;
    f32 jump_acceleration;
    f32 gravity;
    f32 friction;
    TextureID texture_id;
    EntityFlags flags;
    EntityRenderProc *render_proc;
    EntityUpdateProc *update_proc;
};

Entity *spawn_entity();
Entity *get_entity(EntityID id);
void kill_entity(EntityID id);
namespace Internal {
extern Util::IchigoVector<Ichigo::Entity> entities;
}
}