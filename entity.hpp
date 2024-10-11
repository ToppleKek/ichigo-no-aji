#pragma once
#include "common.hpp"
#include "math.hpp"
#include "asset.hpp"
#include "util.hpp"

namespace Ichigo {
struct Entity;
using EntityRenderProc  = void (Entity *);
using EntityUpdateProc  = void (Entity *);
using EntityCollideProc = void (Entity *, Entity *);
using EntityFlags       = u64;

namespace EntityControllers {
void player_controller(Entity *entity);
void patrol_controller(Entity *entity);
}

struct EntityID {
    u32 generation;
    u32 index;
};

enum EntityFlag {
    EF_ON_GROUND = 1 << 0
};

struct Entity {
    char name[8];
    EntityID id;
    Rectangle col;
    Vec2<u32> left_standing_tile;
    Vec2<u32> right_standing_tile;
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
    EntityCollideProc *collide_proc;
};

Entity *spawn_entity();
Entity *get_entity(EntityID id);
void kill_entity(EntityID id);
void move_entity_in_world(Entity *entity);
namespace Internal {
extern Util::IchigoVector<Ichigo::Entity> entities;
char *entity_id_as_string(EntityID entity_id); // TODO: NOT THREAD SAFE!!
}
}
