#pragma once

#include "../ichigo.hpp"

#define PLAYER_STARTING_HEALTH 10.0f

enum EntityType : u32 {
    ET_NOTHING,
    ET_PLAYER,
    ET_GERT,
    ET_COIN,
    ET_ENTRANCE,
    ET_LEVEL_ENTRANCE,
    ET_ENTRANCE_TRIGGER,
    ET_MOVING_PLATFORM,
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

struct __attribute__((packed)) PlayerSaveData {
    f32 health;
    i64 level_id;
    Vec2<f32> position;
    u64 inventory_flags;
    u64 story_flags;
};

struct LevelSaveData {
    u64 progress_flags;
    u64 item_flags;
};

struct GameSaveData {
    PlayerSaveData player_data;
    Bana::FixedArray<LevelSaveData> level_data;
};

extern GameSaveData current_save_data;
extern ProgramState program_state;
extern bool input_disabled;
// extern bool ai_disabled;
extern const Level all_levels[];
extern i64 current_level_id;

using FullscreenTransitionCompleteCallback = void (uptr);
void fullscreen_transition(Vec4<f32> from, Vec4<f32> to, f32 t, FullscreenTransitionCompleteCallback *on_complete, uptr callback_data);
void try_change_level(i64 level_id);
LevelSaveData current_level_save_data();
bool save_game();
bool load_game();
void new_game();
}
