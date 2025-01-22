#pragma once

#include "../ichigo.hpp"

enum EntityType : u32 {
    ET_NOTHING,
    ET_PLAYER,
    ET_GERT,
    ET_COIN,
    ET_ENTRANCE,
    ET_ENTRANCE_TRIGGER,
    ET_SPELL,
};

namespace Ichiaji {
enum ProgramState {
    MAIN_MENU,
    GAME,
    PAUSE,
};

struct BackgroundPngDescriptor {
    usize png_size;
    u8 *png_data;
    Bana::FixedArray<Ichigo::Background> backgrounds;
};

struct Level {
    u8 *tilemap_data;
    Vec4<f32> background_colour;
    Bana::FixedArray<Ichiaji::BackgroundPngDescriptor> background_descriptors;
    Bana::FixedArray<Ichigo::EntityDescriptor> entity_descriptors;
    Bana::FixedArray<Vec2<f32>> entrance_table; // A table indexed by entrance_id (stored in the user data of the entity) to exit position.
};

extern ProgramState program_state;
extern bool input_disabled;
// extern bool ai_disabled;
extern Level current_level;

using FullscreenTransitionCompleteCallback = void (uptr);
void fullscreen_transition(Vec4<f32> from, Vec4<f32> to, f32 t, FullscreenTransitionCompleteCallback *on_complete, uptr callback_data);
}
