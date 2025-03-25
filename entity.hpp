/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Entity functions and structures.

    Author:      Braeden Hong
    Last edited: 2024/11/30
*/

#pragma once
#include "common.hpp"
#include "math.hpp"
#include "asset.hpp"

namespace Ichigo {
struct Entity;

using EntityRenderProc  = void (Entity *);
using EntityUpdateProc  = void (Entity *);
using EntityKillProc    = void (Entity *);
using EntityCollideProc = void (Entity *self, Entity *other, Vec2<f32> collider_normal, Vec2<f32> collision_normal, Vec2<f32> collision_pos);
using EntityStandProc   = void (Entity *self, Entity *other, bool standing);
using EntityFlags       = u64;

namespace EntityControllers {
void player_controller(Entity *entity);
void patrol_controller(Entity *entity);
}

struct EntityID {
    u32 generation;
    u32 index;
};

#define NULL_ENTITY_ID (Ichigo::EntityID{0, 0})

inline bool operator==(const Ichigo::EntityID &lhs, const Ichigo::EntityID &rhs) {
    return lhs.generation == rhs.generation && lhs.index == rhs.index;
}

// TODO: NOTE: The flag EF_ANIM_LOOPING is used to signal that the animation has played out and will now loop
//             from loop_start to loop_end. Do we need this? Right now, the animation plays from the first frame
//             to the last one, and then loops from loop start to loop end. You *could* clear EF_ANIM_LOOPING to
//             play to the end again, but then the animation will just start over again. This doesn't seem very
//             useful.
// NOTE 2:     Yes! This does sound useful for being able to check if an animation has fully played out. Maybe it should
//             be in some separate "animation flags" or something, but it can live here for now.
enum EntityFlag {
    EF_MARKED_FOR_DEATH = 1 << 0,  // This entity will be killed at the end of the frame.
    EF_ON_GROUND        = 1 << 1,  // This entity is on the ground.
    EF_FLIP_H           = 1 << 2,  // This entity renders with its u coordinates reversed (ie. it is horizontally flipped).
    EF_INVISIBLE        = 1 << 3,  // This entity will not render its sprite. However, animation frames still advance.
    EF_TANGIBLE         = 1 << 4,  // This entity acts as a wall/floor/ceiling. Other entities can stand on it.
    EF_TANGIBLE_ON_TOP  = 1 << 5,  // This entity acts only as a floor. Other entities can stand on it, but do not collide with it as a wall or ceiling.
    EF_ANIM_LOOPING     = 1 << 6,  // This entity has played its animation fully once and is now looping.
    // FIXME: Probably not needed? Maybe with smarter camera intersection logic we can make this one flag again?
    EF_BLOCKS_CAMERA_X  = 1 << 7,  // This entity blocks the camera in the x axis from passing over it.
    EF_BLOCKS_CAMERA_Y  = 1 << 8,  // This entity blocks the camera in the y axis from passing over it.
    EF_NO_COLLIDE       = 1 << 9,  // Collision checks are skipped with this entity.
    EF_STATIC           = 1 << 10, // This entity does not move, and should never be moved by other entities.
};

// What happened as a result of moving an entity in the world?
enum EntityMoveResult {
    NO_MOVE, // TODO: Is this any different from NOTHING_SPECIAL??
    NOTHING_SPECIAL,
    HIT_GROUND,
    HIT_WALL,
    HIT_CEILING,
    BECAME_AIRBORNE,
};

struct SpriteSheet {
    u32 cell_width;
    u32 cell_height;
    TextureID texture;
};

struct Animation {
    u32 tag;
    u32 cell_of_first_frame;
    u32 cell_of_last_frame;
    u32 cell_of_loop_start;
    u32 cell_of_loop_end;
    f32 seconds_per_frame;
};

struct Sprite {
    Vec2<f32> pos_offset;
    f32 width;  // NOTE: In metres
    f32 height; // NOTE: In metres
    SpriteSheet sheet;
    Animation animation;
    u32 current_animation_frame;
    f32 elapsed_animation_frame_time;
};

struct EntityDescriptor {
    char name[32];
    u32 type;
    Vec2<f32> pos;
    f32 custom_width;
    f32 custom_height;
    f32 movement_speed;
    i64 data;
};

// FIXME: A lot of these fields are unused in certian use cases.
struct Entity {
    char name[24];
    i8 draw_layer;
    EntityID id;
    Rect<f32> col;
    Vec2<i32> left_standing_tile;
    Vec2<i32> right_standing_tile;
    Vec2<f32> velocity;
    Vec2<f32> acceleration;
    Vec2<f32> max_velocity;
    EntityID standing_entity_id; // The entity that this one is standing on.
    Sprite sprite;
    f32 movement_speed;
    f32 jump_acceleration;
    f32 gravity; // TODO: Stupid.
    f32 friction_when_tangible; // The amount of friction is applied to entites that are walking on this one.
    EntityFlags flags;
    EntityRenderProc *render_proc;
    EntityUpdateProc *update_proc;
    EntityKillProc *kill_proc;
    EntityCollideProc *collide_proc;
    EntityStandProc *stand_proc; // Fired when you are tangible and someone stands/gets off you. TODO: is this stupid?

    union {
        i64 user_data_i64;
        struct {
            f32 user_data_f32_1;
            f32 user_data_f32_2;
        };
    };

    u32 user_type_id;
};

Entity *spawn_entity();
Entity *get_entity(EntityID id);

inline bool entity_is_dead(EntityID id) {
    return get_entity(id) == nullptr;
}

void change_entity_draw_layer(Ichigo::EntityID id, i8 new_layer);
void change_entity_draw_layer(Ichigo::Entity *entity, i8 new_layer);
void conduct_end_of_frame_executions();
void kill_entity(EntityID id);
void kill_entity(Entity *entity);
void kill_entity_deferred(EntityID id);
void kill_entity_deferred(Entity *entity);
void kill_all_entities();
Vec2<f32> calculate_projected_next_position(Ichigo::Entity *entity);
Bana::Optional<Vec2<f32>> nearest_tangible_ground_tile(Ichigo::Entity *entity);
EntityMoveResult move_entity_in_world(Entity *entity);
void teleport_entity_considering_colliders(Entity *entity, Vec2<f32> pos);
namespace Internal {
extern Bana::Array<Ichigo::Entity> entities;
extern Bana::Array<Ichigo::EntityID> entity_ids_in_draw_order;
extern bool entity_draw_order_dirty;

char *entity_id_as_string(EntityID entity_id); // TODO: NOT THREAD SAFE!! - message to past me- nothing really is lol. Who cares right now? We are a 2D game engine that runs at ~3000fps.
}
}
