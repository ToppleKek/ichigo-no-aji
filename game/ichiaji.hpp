#pragma once

#include "../ichigo.hpp"

namespace Ichiaji {
enum ProgramState {
    MAIN_MENU,
    GAME,
    PAUSE,
};

struct UnloadedTilemap {
    u8 *tilemap_data;
    Ichigo::SpriteSheet spritesheet;
};

struct Level {
    Bana::FixedArray<Ichiaji::UnloadedTilemap> rooms;
    Bana::FixedMap<Ichigo::EntityID, i32> entrance_map;
};

extern ProgramState program_state;
extern bool modal_open;
extern Level current_level;
}