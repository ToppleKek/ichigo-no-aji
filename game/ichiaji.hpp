#pragma once

#include "../ichigo.hpp"

#define PLAYER_STARTING_HEALTH 10.0f
#define HEALTH_BONUS_ITEM_POWER 2.0f
#define ATTACK_SPEED_UP_POWER 0.2f
#define ATTACK_POWER_UP_POWER 1.0f
#define COIN_VALUE 20
#define RECOVERY_HEART_VALUE 2.0f

enum EntityType : u32 {
    ET_NOTHING,
    ET_PLAYER,
    ET_GERT,
    ET_COIN,
    ET_ENTRANCE,
    ET_LEVEL_ENTRANCE,
    ET_ENTRANCE_TRIGGER,
    ET_ENTRANCE_TRIGGER_H,
    ET_MOVING_PLATFORM,
    ET_SPELL,
    ET_DEATH_TRIGGER,
    ET_LOCKED_DOOR,
    ET_KEY,
    ET_ELEVATOR,
    ET_RABBIT,
    ET_SHOP_ENTRANCE,
    ET_BUCHOU,
    ET_MINIBOSS_ROOM_CONTROLLER,
    ET_RECOVERY_HEART,
    ET_HP_UP_COLLECTABLE,
    ET_ATTACK_SPEED_UP_COLLECTABLE,
    ET_FIRE_SPELL_COLLECTABLE,
    ET_FIRE_SPELL,
    ET_ICE_BLOCK,
    ET_SAVE_STATUE,
};

enum TileType : u16 {
    TT_AIR,
    TT_CB_TL,
    TT_CB_TR,
    TT_CB_TM,
    TT_SPIKE,
};

namespace Ichiaji {
enum ProgramState {
    PS_MAIN_MENU,
    PS_GAME,
    PS_UI_MENU,
    PS_CUTSCENE,
    PS_PAUSE,
};

using LevelInitProc   = void ();
using LevelUpdateProc = void ();
using LevelSpawnProc  = bool (const Ichigo::EntityDescriptor &);
struct Level {
    u8 *tilemap_data;
    Vec4<f32> background_colour;
    Bana::FixedArray<Ichigo::Background> backgrounds;
    Bana::FixedArray<Ichigo::EntityDescriptor> entity_descriptors;
    LevelInitProc *init_proc;
    LevelUpdateProc *update_proc;
    LevelSpawnProc *spawn_proc;
};

struct __attribute__((packed)) PlayerSaveData {
    f32 health;
    u32 money;
    i64 level_id;
    Vec2<f32> position;
    u64 inventory_flags;
    u64 story_flags;
};

struct __attribute__((packed)) PlayerSaveDataV1 {
    f32 health;
    i64 level_id;
    Vec2<f32> position;
    u64 inventory_flags;
    u64 story_flags;
};

struct __attribute__((packed)) LevelSaveData {
    u64 progress_flags;
    u64 item_flags;
};

struct PlayerBonuses {
    f32 max_health;
    f32 attack_speed;
    f32 attack_power;
};

struct GameSaveData {
    PlayerSaveData player_data;
    Bana::FixedArray<LevelSaveData> level_data;
};

enum InventoryItem {
    INV_SHOP_HEALTH_UPGRADE           = 1 << 0,
    INV_SHOP_ATTACK_SPEED_UPGRADE     = 1 << 1,
    INV_SHOP_ATTACK_POWER_UPGRADE     = 1 << 2,
    INV_HP_UP_COLLECTABLE_1           = 1 << 3,
    INV_ATTACK_SPEED_UP_COLLECTABLE_1 = 1 << 4,
    INV_FIRE_SPELL                    = 1 << 5,
};

extern GameSaveData current_save_data;
extern PlayerBonuses player_bonuses;
extern ProgramState program_state;
extern bool input_disabled;
// extern bool ai_disabled;
extern Level all_levels[];
extern i64 current_level_id;
extern Ichigo::EntityID player_entity_id;

using FullscreenTransitionCompleteCallback = void (uptr);
void fullscreen_transition(Vec4<f32> from, Vec4<f32> to, f32 t, FullscreenTransitionCompleteCallback *on_complete, uptr callback_data);
void try_change_level(i64 level_id);
void try_talk_to(Ichigo::Entity *entity);
void recalculate_player_bonuses();
void drop_collectable(Vec2<f32> pos);

inline bool item_obtained(InventoryItem item) {
    return FLAG_IS_SET(Ichiaji::current_save_data.player_data.inventory_flags, item);
}

inline bool give_item(InventoryItem item) {
    if (item_obtained(item)) return false;

    SET_FLAG(Ichiaji::current_save_data.player_data.inventory_flags, item);
    recalculate_player_bonuses();
    return true;
}

bool save_game();
bool load_game();
void new_game();

inline LevelSaveData &current_level_save_data() {
    return current_save_data.level_data[current_level_id];
}

inline Level &current_level() {
    return all_levels[current_level_id];
}
}
