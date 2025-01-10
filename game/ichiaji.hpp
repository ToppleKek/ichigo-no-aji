#pragma once

#include "../ichigo.hpp"

namespace Ichiaji {
enum ProgramState {
    MAIN_MENU,
    GAME,
    PAUSE,
};

enum EntityType {
    ET_NOTHING,
    ET_PLAYER,
    ET_GERT,
    ET_COIN,
    ET_ENTRANCE,
};

struct EntityDescriptor {
    EntityType type;
    Vec2<f32> pos;
    i64 data;
};

struct Level {
    u8 *tilemap_data;
    Bana::FixedArray<Ichiaji::EntityDescriptor> entity_descriptors;
    Bana::FixedMap<i64, Vec2<f32>> entrance_map; // A map from entrance id (stored in the user data of the entity) to exit position.
};

extern ProgramState program_state;
// extern bool input_disabled;
// extern bool ai_disabled;
extern Level current_level;

using FullscreenTransitionCompleteCallback = void (void *);
void fullscreen_transition(Vec4<f32> from, Vec4<f32> to, f32 t, FullscreenTransitionCompleteCallback *on_complete, void *callback_data);
}
