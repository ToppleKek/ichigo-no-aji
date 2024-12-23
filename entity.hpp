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
#include "util.hpp"

namespace Ichigo {
struct Entity;

using EntityRenderProc  = void (Entity *);
using EntityUpdateProc  = void (Entity *);
using EntityCollideProc = void (Entity *, Entity *, Vec2<f32>, Vec2<f32>);
using EntityFlags       = u64;

namespace EntityControllers {
void player_controller(Entity *entity);
void patrol_controller(Entity *entity);
}

struct EntityID {
    u32 generation;
    u32 index;
};

inline bool operator==(const Ichigo::EntityID &lhs, const Ichigo::EntityID &rhs) {
    return lhs.generation == rhs.generation && lhs.index == rhs.index;
}

// TODO: NOTE: The flag EF_ANIM_LOOPING is used to signal that the animation has played out and will now loop
//             from loop_start to loop_end. Do we need this? Right now, the animation plays from the first frame
//             to the last one, and then loops from loop start to loop end. You *could* clear EF_ANIM_LOOPING to
//             play to the end again, but then the animation will just start over again. This doesn't seem ver
//             useful.
enum EntityFlag {
    EF_MARKED_FOR_DEATH = 1 << 0, // This entity will be killed at the end of the frame
    EF_ON_GROUND        = 1 << 1, // This entity is on the ground
    EF_FLIP_H           = 1 << 2, // This entity renders with its u coordinates reversed (ie. it is horizontally flipped).
    EF_INVISIBLE        = 1 << 3, // This entity will not render its sprite. However, animation frames still advance.
    // EF_ANIM_LOOPING = 1 << 4, // TODO: Not sure if this makes sense?
};

// What happened as a result of moving an entity in the world?
enum EntityMoveResult {
    NO_MOVE,
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
    f32 width;
    f32 height;
    SpriteSheet sheet;
    Animation animation;
    u32 current_animation_frame;
    f32 elapsed_animation_frame_time;
};

struct Entity {
    char name[8];
    EntityID id;
    Rect<f32> col;
    Vec2<u32> left_standing_tile;
    Vec2<u32> right_standing_tile;
    Vec2<f32> velocity;
    Vec2<f32> acceleration;
    Vec2<f32> max_velocity;
    Sprite sprite;
    f32 movement_speed;
    f32 jump_acceleration;
    f32 gravity; // TODO: Stupid.
    EntityFlags flags;
    EntityRenderProc *render_proc;
    EntityUpdateProc *update_proc;
    EntityCollideProc *collide_proc;
};

Entity *spawn_entity();
Entity *get_entity(EntityID id);
void conduct_end_of_frame_executions();
void kill_entity(EntityID id);
void kill_entity_deferred(EntityID id);
void kill_all_entities();
EntityMoveResult move_entity_in_world(Entity *entity);
namespace Internal {
extern Util::IchigoVector<Ichigo::Entity> entities;
char *entity_id_as_string(EntityID entity_id); // TODO: NOT THREAD SAFE!! - message to past me- nothing really is lol. Who cares right now? We are a 2D game engine that runs at ~3000fps.
}
}
