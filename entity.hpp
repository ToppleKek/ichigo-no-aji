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
    RectangleCollider col;
    Vec2<f32> velocity;
    Vec2<f32> acceleration;
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