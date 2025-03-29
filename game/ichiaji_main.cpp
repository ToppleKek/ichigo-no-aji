/*
    Ichigo demo game code.

    Game init and logic.

    Author: Braeden Hong
    Date:   2025/03/12
*/

#include "../ichigo.hpp"
#include "ichiaji.hpp"
#include "strings.hpp"
#include "irisu.hpp"
#include "moving_platform.hpp"
#include "particle_source.hpp"
#include "entrances.hpp"
#include "rabbit_enemy.hpp"
#include "ui.hpp"
#include "asset_catalog.hpp"
#include "miniboss_room.hpp"
#include "collectables.hpp"
#include "../util.hpp"

// Levels
#include "levels/level0.ichigolvl"
#include "levels/level1.ichigolvl"
#include "levels/cave.ichigolvl"
#include "levels/first.ichigolvl"

Ichiaji::Level Ichiaji::all_levels[] = {
    Level0::level,
    Level1::level,
    CaveEntranceLevel::level,
    FirstLevel::level,
};

static Vec4<f32> colour_white = {1.0f, 1.0f, 1.0f, 1.0f};

static Ichigo::TextStyle title_style = {
    .alignment    = Ichigo::TextAlignment::CENTER,
    .scale        = 1.5f,
    .colour       = colour_white,
    .line_spacing = 100.0f
};

static Ichigo::TextStyle menu_item_style = {
    .alignment    = Ichigo::TextAlignment::CENTER,
    .scale        = 1.0f,
    .colour       = colour_white,
    .line_spacing = 200.0f
};

static Ichigo::TextStyle info_style = {
    .alignment    = Ichigo::TextAlignment::RIGHT,
    .scale        = 0.5f,
    .colour       = colour_white,
    .line_spacing = 100.0f
};

static Ichigo::TextStyle credit_style = {
    .alignment    = Ichigo::TextAlignment::LEFT,
    .scale        = 0.5f,
    .colour       = colour_white,
    .line_spacing = 100.0f
};

static Ichigo::SpriteSheet tileset_sheet;

static Bana::Array<Ichigo::EntityDescriptor> current_entity_descriptors{};

struct Coin {
    bool collected;
    Ichigo::EntityID id;
};

Language               current_language           = ENGLISH;
Ichiaji::GameSaveData  Ichiaji::current_save_data = {};
Ichiaji::Bgm           Ichiaji::current_bgm       = {};
Ichiaji::PlayerBonuses Ichiaji::player_bonuses    = {};
Ichiaji::ProgramState  Ichiaji::program_state     = Ichiaji::PS_MAIN_MENU;
bool                   Ichiaji::input_disabled    = false;
i64                    Ichiaji::current_level_id  = 0;
Ichigo::EntityID       Ichiaji::player_entity_id  = NULL_ENTITY_ID;

#ifdef ICHIGO_DEBUG
static f32 DEBUG_timescale = 1.0f;
#endif

static void gert_update(Ichigo::Entity *gert) {
    if (Ichiaji::program_state != Ichiaji::PS_GAME) {
        return;
    }

    Ichigo::EntityControllers::patrol_controller(gert);
}

static void gert_collide(Ichigo::Entity *gert, Ichigo::Entity *other, [[maybe_unused]] Vec2<f32> normal, [[maybe_unused]] Vec2<f32> collision_normal, [[maybe_unused]] Vec2<f32> collision_pos) {
    if (other->user_type_id == ET_SPELL || other->user_type_id == ET_FIRE_SPELL) {
        Ichiaji::drop_collectable(gert->col.pos);
        Ichigo::Mixer::play_audio_oneshot(Assets::gert_death_audio_id, 1.0f, 1.0f, 1.0f);
        Ichigo::kill_entity_deferred(gert->id);
    }
}

static bool controller_connected = false;

static void spawn_gert(Vec2<f32> pos) {
    Ichigo::Entity *enemy = Ichigo::spawn_entity();
    std::strcpy(enemy->name, "gert");

    enemy->col            = {pos, 0.7f, 0.8f};
    enemy->max_velocity   = {2.0f, 12.0f};
    enemy->movement_speed = 6.0f;
    enemy->gravity        = 9.8f;
    enemy->update_proc    = gert_update;
    enemy->collide_proc   = gert_collide;
    enemy->user_type_id   = ET_GERT;

    Ichigo::Animation gert_idle   = {};
    gert_idle.cell_of_first_frame = 0;
    gert_idle.cell_of_last_frame  = 4;
    gert_idle.cell_of_loop_start  = 0;
    gert_idle.cell_of_loop_end    = 4;
    gert_idle.seconds_per_frame   = 0.08f;

    Ichigo::Sprite gert_sprite    = {};
    gert_sprite.width             = 1.0f;
    gert_sprite.height            = 1.0f;
    gert_sprite.pos_offset        = Util::calculate_centered_pos_offset(enemy->col.w, enemy->col.h, gert_sprite.width, gert_sprite.height);
    gert_sprite.sheet.texture     = Assets::gert_texture_id;
    gert_sprite.sheet.cell_width  = 32;
    gert_sprite.sheet.cell_height = 32;
    gert_sprite.animation         = gert_idle;

    enemy->sprite = gert_sprite;
}

static void spawn_buchou(const Ichigo::EntityDescriptor &descriptor) {
    Ichigo::Entity *e = Ichigo::spawn_entity();
    std::strcpy(e->name, "buchou");

    // TODO @asset: Sprite for this guy.
    e->col            = {descriptor.pos, 1.0f, 1.0f};
    e->user_type_id   = ET_BUCHOU;
}

static void ice_block_update(Ichigo::Entity *e) {
    if (e->user_data_f32_2 != 0.0f) {
        e->user_data_f32_1 -= Ichigo::Internal::dt;
    }
}

static void ice_block_on_collide(Ichigo::Entity *e, Ichigo::Entity *other, [[maybe_unused]] Vec2<f32> normal, [[maybe_unused]] Vec2<f32> collision_normal, [[maybe_unused]] Vec2<f32> collision_pos) {
    if (other->user_type_id == ET_FIRE_SPELL && e->user_data_f32_2 == 0.0f) {
        Rect<f32> camera_rect = {-1.0f * get_translation2d(Ichigo::Camera::transform), Ichigo::Camera::screen_tile_dimensions.x, Ichigo::Camera::screen_tile_dimensions.y};
        f32 d = e->col.pos.x - (camera_rect.pos.x + camera_rect.w / 2.0f);
        f32 l, r;
        if (d < 0.0f) {
            l = 1.0f;
            r = safe_ratio_0(1.0f, -d);
        } else  {
            l = safe_ratio_0(1.0f, d);
            r = 1.0f;
        }

        Ichigo::Mixer::play_audio_oneshot(Assets::steam_audio_id, 1.0f, clamp(l, 0.25f, 1.0f), clamp(r, 0.25f, 1.0f));
        CLEAR_FLAG(e->flags, Ichigo::EF_TANGIBLE);
        e->user_data_f32_1 = 1.0f;
        e->user_data_f32_2 = 1.0f;
    }
}

static void ice_block_render(Ichigo::Entity *e) {
    f32 alpha = 1.0f;
    if (e->user_data_f32_2 != 0.0f) {
        alpha = ichigo_lerp(1.0f, 1.0f - (e->user_data_f32_1 / e->user_data_f32_2), 0.0f);
    }

    if (alpha <= 0.0f) {
        Ichigo::kill_entity_deferred(e);
    }

    Ichigo::render_sprite_and_advance_animation(Ichigo::WORLD, e->col.pos, &e->sprite, false, true, m4identity_f32, {1.0f, 1.0f, 1.0f, alpha});
}

static void spawn_ice_block(const Ichigo::EntityDescriptor &descriptor) {
    static constexpr u32 animation_frames = 9;
    Ichigo::Sprite sprite = {
        {},
        1.0f,
        1.0f,
        { 32, 32, Assets::ice_block_texture_id },
        {
            0,
            0,
            animation_frames - 1,
            0,
            animation_frames - 1,
            0.2f
        },
        0,
        0.0f
    };

    Ichigo::Entity *e = Ichigo::spawn_entity();
    std::strcpy(e->name, "ice_block");

    e->col                    = {descriptor.pos, 1.0f, 1.0f};
    e->sprite                 = sprite;
    e->user_type_id           = ET_ICE_BLOCK;
    e->update_proc            = ice_block_update;
    e->collide_proc           = ice_block_on_collide;
    e->render_proc            = ice_block_render;
    e->friction_when_tangible = 1.5f;

    SET_FLAG(e->flags, Ichigo::EF_TANGIBLE);
    SET_FLAG(e->flags, Ichigo::EF_STATIC);
}

static void spawn_save_statue(const Ichigo::EntityDescriptor &descriptor) {
    static constexpr u32 animation_frames = 6;
    Ichigo::Sprite sprite = {
        {},
        1.0f,
        2.0f,
        { 32, 64, Assets::save_statue_texture_id },
        {
            0,
            0,
            animation_frames - 1,
            0,
            animation_frames - 1,
            0.2f
        },
        0,
        0.0f
    };

    Ichigo::Entity *e = Ichigo::spawn_entity();
    std::strcpy(e->name, "save_statue");

    e->col                    = {descriptor.pos, sprite.width, sprite.height};
    e->sprite                 = sprite;
    e->user_type_id           = ET_SAVE_STATUE;

    SET_FLAG(e->flags, Ichigo::EF_STATIC);
}

static void respawn_all_entities(const Bana::Array<Ichigo::EntityDescriptor> &descriptors, bool load_player_data_from_save) {
    for (u32 i = 0; i < descriptors.size; ++i) {
        const auto &d = descriptors[i];

        if (Ichiaji::all_levels[Ichiaji::current_level_id].spawn_proc) {
            if (Ichiaji::all_levels[Ichiaji::current_level_id].spawn_proc(d)) continue;
        }

        switch (d.type) {
            case ET_PLAYER: {
                Ichigo::Entity *player = Ichigo::spawn_entity();
                Irisu::spawn(player, load_player_data_from_save ? Ichiaji::current_save_data.player_data.position : d.pos);
                Ichiaji::player_entity_id = player->id;
                Ichigo::Camera::mode = Ichigo::Camera::Mode::FOLLOW;
                Ichigo::Camera::follow(player->id);
            } break;

            case ET_GERT: {
                spawn_gert(d.pos);
            } break;

            case ET_COIN: {

            } break;

            case ET_ENTRANCE:
            case ET_LEVEL_ENTRANCE: {
                Entrances::spawn_entrance(d);
            } break;

            case ET_ENTRANCE_TRIGGER_H:
            case ET_ENTRANCE_TRIGGER: {
                Entrances::spawn_entrance_trigger(d);
            } break;

            case ET_ELEVATOR:
            case ET_MOVING_PLATFORM: {
                MovingPlatform::spawn(d);
            } break;

            case ET_DEATH_TRIGGER: {
                Entrances::spawn_death_trigger(d);
            } break;

            case ET_KEY: {
                if (!FLAG_IS_SET(Ichiaji::current_level_save_data().progress_flags, d.data)) {
                    Entrances::spawn_key(d);
                }
            } break;

            case ET_RABBIT: {
                RabbitEnemy::spawn(d);
            } break;

            case ET_SHOP_ENTRANCE: {
                Entrances::spawn_shop_entrance(d);
            } break;

            case ET_BUCHOU: {
                spawn_buchou(d);
            } break;

            case ET_MINIBOSS_ROOM_CONTROLLER: {
                Miniboss::spawn_controller_entity(d);
            } break;

            case ET_HP_UP_COLLECTABLE:
            case ET_ATTACK_SPEED_UP_COLLECTABLE: {
                if (!item_obtained((Ichiaji::InventoryItem) d.data)) {
                    Collectables::spawn_powerup(d);
                }
            } break;

            case ET_FIRE_SPELL_COLLECTABLE: {
                if (!item_obtained(Ichiaji::INV_FIRE_SPELL)) {
                    Collectables::spawn_fire_spell_collectable(d);
                }
            } break;

            case ET_ICE_BLOCK: {
                spawn_ice_block(d);
            } break;

            case ET_SAVE_STATUE: {
                spawn_save_statue(d);
            } break;

            default: {
                ICHIGO_ERROR("Invalid/unknown entity type: %d", d.type);
            }
        }
    }
}

static void change_level(i64 level_id, bool first_start) {
    Ichigo::kill_all_entities();
    std::memset(&Ichigo::game_state.background_layers, 0, sizeof(Ichigo::game_state.background_layers));

    Ichiaji::current_level_id = level_id;
    Ichiaji::Level level      = Ichiaji::all_levels[level_id];
    Ichigo::set_tilemap(level.tilemap_data, tileset_sheet);

    // == Setup backgrounds ==
    Ichigo::game_state.background_colour = level.background_colour;
    for (u32 i = 0; i < level.backgrounds.size; ++i) {
        Ichigo::game_state.background_layers[i] = level.backgrounds[i];
    }

    // NOTE: "current_entity_descriptors" is copied to a Bana::Array because the editor may need to modify it.
    current_entity_descriptors.size = level.entity_descriptors.size;
    current_entity_descriptors.ensure_capacity(level.entity_descriptors.size);
    std::memcpy(current_entity_descriptors.data, level.entity_descriptors.data, level.entity_descriptors.size * sizeof(Ichigo::EntityDescriptor));

    if (level.enter_proc) level.enter_proc();

    // == Spawn entities in world ==
    respawn_all_entities(current_entity_descriptors, first_start);
}

static void init_game() {
    // == SETUP CURRENT LEVEL ==
    assert(Ichiaji::current_save_data.player_data.level_id < (i64) ARRAY_LEN(Ichiaji::all_levels));
    if (Ichiaji::current_save_data.player_data.level_id < 0) {
         // NOTE: Load the first level if this is a new file. Do not attempt to spawn the player at the last save point
         //       because the last save point of a new file is 0,0.
        change_level(1, false);
        Ichiaji::current_save_data.player_data.level_id = 1;
    } else {
        change_level(Ichiaji::current_save_data.player_data.level_id, true);
    }
}

static void deinit_game() {
    Ichigo::Camera::mode = Ichigo::Camera::Mode::MANUAL;
    Ichiaji::change_bgm({0, 0.0f, 0.0f});

    change_level(0, false);
}

bool Ichiaji::save_game() {
    Ichigo::Internal::PlatformFile *save_file = Ichigo::Internal::platform_open_file_write(Bana::temp_string("./default.save"));

    Ichiaji::current_save_data.player_data.level_id = Ichiaji::current_level_id;

    if (!save_file) {
        return false;
    }

    static const u16 SAVE_FILE_VERSION_NUMBER = 2;

    Ichigo::Internal::platform_append_file_sync(save_file, (u8 *) &SAVE_FILE_VERSION_NUMBER, sizeof(SAVE_FILE_VERSION_NUMBER));
    Ichigo::Internal::platform_append_file_sync(save_file, (u8 *) &Ichiaji::current_save_data.player_data, sizeof(Ichiaji::PlayerSaveData));

    usize num_levels = Ichiaji::current_save_data.level_data.size;
    Ichigo::Internal::platform_append_file_sync(save_file, (u8 *) &num_levels, sizeof(num_levels));
    Ichigo::Internal::platform_append_file_sync(save_file, (u8 *) Ichiaji::current_save_data.level_data.data, sizeof(Ichiaji::LevelSaveData) * num_levels);

    Ichigo::Internal::platform_close_file(save_file);

    return true;
}

bool Ichiaji::load_game() {
    auto read_result = Ichigo::Internal::platform_read_entire_file_sync(Bana::temp_string("./default.save"), Ichigo::Internal::temp_allocator);
    if (!read_result.has_value) {
        ICHIGO_ERROR("Failed to read save file.");
        return false;
    }

    auto data = read_result.value;

    Bana::BufferReader br = {(char *) data.data, (usize) data.size, 0};

#define ASSIGN_OR_FAIL(DST, OPTIONAL)                                                           \
    do {                                                                                        \
        auto temp_result = OPTIONAL;                                                            \
        if (!temp_result.has_value) return false; else DST = (decltype(DST)) temp_result.value; \
    } while (false)                                                                             \

    u16 version_number = 0;
    ASSIGN_OR_FAIL(version_number, br.read16());

    // TODO: This is something isn't it.
    ICHIGO_INFO("Loading save V%d", version_number);
    switch (version_number) {
        case 1: {
            Ichiaji::PlayerSaveDataV1 *p = nullptr;

            ASSIGN_OR_FAIL(p, br.read_bytes(sizeof(Ichiaji::PlayerSaveDataV1)));

            Ichiaji::current_save_data.player_data.health          = p->health;
            Ichiaji::current_save_data.player_data.money           = 0;
            Ichiaji::current_save_data.player_data.level_id        = p->level_id;
            Ichiaji::current_save_data.player_data.position        = p->position;
            Ichiaji::current_save_data.player_data.inventory_flags = p->inventory_flags;
            Ichiaji::current_save_data.player_data.story_flags     = p->story_flags;

            isize file_num_levels = 0;
            ASSIGN_OR_FAIL(file_num_levels, br.read64());
            isize game_num_levels = ARRAY_LEN(all_levels);

            if (file_num_levels != game_num_levels) {
                ICHIGO_ERROR("The number of levels in this save file does not equal the number of levels registered in the game! Reading the maximum possible number of levels.");
            }

            if (Ichiaji::current_save_data.level_data.data == nullptr) {
                Ichiaji::current_save_data.level_data = Bana::make_fixed_array<Ichiaji::LevelSaveData>(game_num_levels, Ichigo::Internal::perm_allocator);
            }

            assert(game_num_levels == Ichiaji::current_save_data.level_data.capacity);

            usize level_data_bytes_to_read = MIN(Ichiaji::current_save_data.level_data.capacity, file_num_levels) * sizeof(Ichiaji::LevelSaveData);
            if (br.cursor + level_data_bytes_to_read > br.size) return false;

            std::memcpy(Ichiaji::current_save_data.level_data.data, br.current_ptr(), level_data_bytes_to_read);
            Ichiaji::current_save_data.level_data.size = game_num_levels;

            return true;
        } break;

        case 2: {
            Ichiaji::PlayerSaveData *p = nullptr;

            ASSIGN_OR_FAIL(p, br.read_bytes(sizeof(Ichiaji::PlayerSaveData)));

            Ichiaji::current_save_data.player_data = *p;

            isize file_num_levels = 0;
            ASSIGN_OR_FAIL(file_num_levels, br.read64());
            isize game_num_levels = ARRAY_LEN(all_levels);

            if (file_num_levels != game_num_levels) {
                ICHIGO_ERROR("The number of levels in this save file does not equal the number of levels registered in the game! Reading the maximum possible number of levels.");
            }

            if (Ichiaji::current_save_data.level_data.data == nullptr) {
                Ichiaji::current_save_data.level_data = Bana::make_fixed_array<Ichiaji::LevelSaveData>(game_num_levels, Ichigo::Internal::perm_allocator);
            }

            assert(game_num_levels == Ichiaji::current_save_data.level_data.capacity);

            usize level_data_bytes_to_read = MIN(Ichiaji::current_save_data.level_data.capacity, file_num_levels) * sizeof(Ichiaji::LevelSaveData);
            if (br.cursor + level_data_bytes_to_read > br.size) return false;

            std::memcpy(Ichiaji::current_save_data.level_data.data, br.current_ptr(), level_data_bytes_to_read);
            Ichiaji::current_save_data.level_data.size = game_num_levels;

            return true;
        } break;

        default: {
            ICHIGO_ERROR("Unsupported save version");
            return false;
        } break;
    }

#undef ASSIGN_OR_FAIL
}

void Ichiaji::new_game() {
    Ichiaji::current_save_data.player_data.health          = PLAYER_STARTING_HEALTH;
    Ichiaji::current_save_data.player_data.money           = 0;
    Ichiaji::current_save_data.player_data.level_id        = -1; // NOTE: This is set to signify that it is a new file
    Ichiaji::current_save_data.player_data.position        = {};
    Ichiaji::current_save_data.player_data.inventory_flags = 0;
    Ichiaji::current_save_data.player_data.story_flags     = 0;

    if (Ichiaji::current_save_data.level_data.data == nullptr) {
        Ichiaji::current_save_data.level_data = Bana::make_fixed_array<Ichiaji::LevelSaveData>(ARRAY_LEN(all_levels), Ichigo::Internal::perm_allocator);
    }

    Ichiaji::current_save_data.level_data.size = ARRAY_LEN(all_levels);

    std::memset(Ichiaji::current_save_data.level_data.data, 0, Ichiaji::current_save_data.level_data.size * sizeof(Ichiaji::LevelSaveData));
}

void Ichiaji::restart_game_from_save_on_disk() {
    deinit_game();

    if (!load_game()) {
        new_game();
    }

    init_game();
}

void Ichigo::Game::init() {
    std::srand((u64) Ichigo::Internal::platform_get_current_time());

    // == Load save ==
    if (Ichigo::Internal::platform_file_exists("./default.save")) {
        if (Ichiaji::load_game()) {
            Ichigo::show_info("Loaded game from './default.save'.");
        } else {
            Ichigo::show_info("'./default.save' is invalid or corrupt. Starting a new game.");
            Ichiaji::new_game();
        }
    } else {
        Ichiaji::new_game();
    }

    // == Load assets ==
    f64 before = Ichigo::Internal::platform_get_current_time();
    Assets::load_assets();
    ICHIGO_INFO("Asset load took %fms!", (Ichigo::Internal::platform_get_current_time() - before) * 1000.0f);

    Ichiaji::recalculate_player_bonuses();

    Irisu::init();
    MovingPlatform::init();
    ParticleSource::init();
    Entrances::init();
    RabbitEnemy::init();

    tileset_sheet = { 32, 32, Assets::tileset_texture_id };

    // == Load levels ==
    for (u32 i = 0; i < ARRAY_LEN(Ichiaji::all_levels); ++i) {
        auto &level = Ichiaji::all_levels[i];

        if (level.init_proc) level.init_proc();
    }

    // Title screen level
    Ichiaji::current_level_id = 0;

    Ichigo::Camera::mode = Ichigo::Camera::Mode::MANUAL;
    Ichigo::Camera::screen_tile_dimensions = {16.0f, 9.0f};
}

void Ichiaji::try_change_level(i64 level_id) {
    if (level_id >= (i64) ARRAY_LEN(all_levels)) {
        ICHIGO_ERROR("try_change_level: Invalid level index.");
        return;
    }

    change_level(level_id, false);
}

void Ichiaji::try_talk_to(Ichigo::Entity *entity) {
    switch (entity->user_type_id) {
        case ET_BUCHOU: {
            if (!FLAG_IS_SET(current_save_data.level_data[1].progress_flags, Level1::LOCKED_DOOR_1_FLAGS)) {
                SET_FLAG(current_save_data.level_data[1].progress_flags, Level1::LOCKED_DOOR_1_FLAGS);
                Ui::open_dialogue_ui(BUCHOU_DIALOGUE, ARRAY_LEN(BUCHOU_DIALOGUE));
            } else {
                Ui::open_dialogue_ui(BUCHOU_DIALOGUE_HAVE_KEY, ARRAY_LEN(BUCHOU_DIALOGUE_HAVE_KEY));
            }
        } break;

        default: {
            ICHIGO_ERROR("Cannot talk to this entity: %s", entity->name);
        } break;
    }
}

void Ichiaji::recalculate_player_bonuses() {
    Ichiaji::player_bonuses = {};

    if (item_obtained(INV_SHOP_HEALTH_UPGRADE))           Ichiaji::player_bonuses.max_health   += HEALTH_BONUS_ITEM_POWER;
    if (item_obtained(INV_SHOP_ATTACK_SPEED_UPGRADE))     Ichiaji::player_bonuses.attack_speed += ATTACK_SPEED_UP_POWER;
    if (item_obtained(INV_SHOP_ATTACK_POWER_UPGRADE))     Ichiaji::player_bonuses.attack_power += ATTACK_POWER_UP_POWER;
    if (item_obtained(INV_HP_UP_COLLECTABLE_1))           Ichiaji::player_bonuses.max_health   += HEALTH_BONUS_ITEM_POWER;
    if (item_obtained(INV_ATTACK_SPEED_UP_COLLECTABLE_1)) Ichiaji::player_bonuses.attack_speed += ATTACK_SPEED_UP_POWER;
}

void Ichiaji::drop_collectable(Vec2<f32> pos) {
    if (Ichiaji::current_save_data.player_data.health < PLAYER_STARTING_HEALTH + Ichiaji::player_bonuses.max_health && rand_01_f32() < 0.75f) {
        Collectables::spawn_recovery_heart(pos);
    } else {
        Collectables::spawn_coin(pos);
    }
}

void Ichiaji::change_bgm(Bgm new_bgm) {
    static Ichigo::Mixer::PlayingAudioID current_playing_bgm;
    Ichigo::Mixer::cancel_audio(current_playing_bgm);

    if (new_bgm.audio_id != 0) {
        current_playing_bgm  = Ichigo::Mixer::play_audio(new_bgm.audio_id, 1.0f, 1.0f, 1.0f, new_bgm.loop_start, new_bgm.loop_end);
        Ichiaji::current_bgm = new_bgm;
    } else {
        Ichiaji::current_bgm = {};
    }
}

static void draw_game_ui() {
    static u32 last_money = UINT32_MAX;
    static f32 money_ui_t = 0.0f;

    static const Ichigo::TextStyle ui_style = {
        .alignment    = Ichigo::TextAlignment::CENTER,
        .scale        = 0.75f,
        .colour       = {0.0f, 0.0f, 0.0f, 1.0f},
        .line_spacing = 100.0f
    };

    static const Ichigo::TextStyle ui_style_white = {
        .alignment    = Ichigo::TextAlignment::LEFT,
        .scale        = 1.25f,
        .colour       = {1.0f, 1.0f, 1.0f, 1.0f},
        .line_spacing = 100.0f
    };

    f32 player_max_health = PLAYER_STARTING_HEALTH + Ichiaji::player_bonuses.max_health;

    static Ichigo::Texture health_ui_tex   = Ichigo::Internal::textures[Assets::health_bar_ui_texture_id];
    static const Rect<f32> health_ui_rect  = {{0.2f, 0.2f}, pixels_to_metres(health_ui_tex.width), pixels_to_metres(health_ui_tex.height)};
    static const Vec2<f32> health_text_pos = {health_ui_rect.pos.x + health_ui_rect.w / 2.0f, (health_ui_rect.pos.y + health_ui_rect.h / 2.0f) + 0.1f};
    static f32 current_health_bar_pos      = 0.0f;

    static Ichigo::DrawCommand health_text_background_cmd = {
        .type              = Ichigo::DrawCommandType::TEXTURED_RECT,
        .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
        .transform         = m4identity_f32,
        .texture_rect      = health_ui_rect,
        .texture_id        = Assets::health_bar_ui_texture_id,
        .texture_tint      = COLOUR_WHITE
    };

    f32 target_health_bar_pos = ichigo_lerp(0.0f, Ichiaji::current_save_data.player_data.health / player_max_health, health_ui_rect.w - pixels_to_metres(4.0f));

    // Here is an alternative easing function that looks nice but speeds up as health drops.
    // ichigo_lerp(10.0f, safe_ratio_1(MIN(current_health_bar_pos, target_health_bar_pos), MAX(current_health_bar_pos, target_health_bar_pos)), 0.2f)
    current_health_bar_pos = move_towards(
        current_health_bar_pos,
        target_health_bar_pos,
        (0.25f + (DISTANCE(current_health_bar_pos, target_health_bar_pos) * 4.0f)) * Ichigo::Internal::dt
    );

    Rect<f32> health_bar_rect = {
        {health_ui_rect.pos + Vec2<f32>{pixels_to_metres(2.0f), pixels_to_metres(2.0f)}},
        current_health_bar_pos,
        pixels_to_metres(12.0f)
    };

    Ichigo::DrawCommand health_bar_cmd = {
        .type              = Ichigo::DrawCommandType::SOLID_COLOUR_RECT,
        .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
        .transform         = m4identity_f32,
        .rect              = health_bar_rect,
        .colour            = {224.0f / 255.0f, 215.0f / 255.0f, 161.0f / 255.0f, 1.0f}
    };

    Ichigo::push_draw_command(health_text_background_cmd);
    Ichigo::push_draw_command(health_bar_cmd);

    Bana::String health_string = Bana::make_string(64, Ichigo::Internal::temp_allocator);
    Bana::string_format(health_string, "%.1f / %.1f", Ichiaji::current_save_data.player_data.health, player_max_health);

    if (last_money != Ichiaji::current_save_data.player_data.money) {
        money_ui_t = 5.0f;
        last_money = Ichiaji::current_save_data.player_data.money;
    }

    if (money_ui_t > 0.0f) {
        Bana::String money_string = Bana::make_string(64, Ichigo::Internal::temp_allocator);
        Bana::string_format(money_string, "$%u", Ichiaji::current_save_data.player_data.money);

        Vec2<f32> money_ui_pos = {health_ui_rect.pos.x, SCREEN_ASPECT_FIX_HEIGHT - 1.0f};

        if (money_ui_t > 4.85f) {
            money_ui_pos.y = bezier(SCREEN_ASPECT_FIX_HEIGHT - 0.5f, SCREEN_ASPECT_FIX_HEIGHT - 0.5f, (5.0f - money_ui_t) / 0.15f, SCREEN_ASPECT_FIX_HEIGHT - 1.3f, SCREEN_ASPECT_FIX_HEIGHT - 1.0f);
        } else if (money_ui_t < 1.0f) {
            money_ui_pos.y = bezier(SCREEN_ASPECT_FIX_HEIGHT + 5.0f, SCREEN_ASPECT_FIX_HEIGHT + 5.0f, money_ui_t, SCREEN_ASPECT_FIX_HEIGHT - 2.0f, SCREEN_ASPECT_FIX_HEIGHT - 1.0f);
        }

        Ichigo::DrawCommand money_bg_cmd = {
            .type              = Ichigo::DrawCommandType::SOLID_COLOUR_RECT,
            .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
            .transform         = m4identity_f32,
            .rect              = {money_ui_pos, get_text_width(money_string.data, money_string.length, ui_style_white) + 0.5f, 0.75f},
            .colour            = {0.0f, 0.0f, 0.0f, 0.9f}
        };

        Ichigo::DrawCommand money_text_cmd = {
            .type              = Ichigo::DrawCommandType::TEXT,
            .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
            .transform         = m4identity_f32,
            .string            = money_string.data,
            .string_length     = money_string.length,
            .string_pos        = money_ui_pos + Vec2<f32>{0.25f, 0.5f},
            .text_style        = ui_style_white
        };

        Ichigo::push_draw_command(money_bg_cmd);
        Ichigo::push_draw_command(money_text_cmd);

        money_ui_t -= Ichigo::Internal::dt;
    }

    Ichigo::DrawCommand health_text_cmd = {
        .type              = Ichigo::DrawCommandType::TEXT,
        .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
        .transform         = m4identity_f32,
        .string            = health_string.data,
        .string_length     = health_string.length,
        .string_pos        = health_text_pos,
        .text_style        = ui_style
    };

    Ichigo::push_draw_command(health_text_cmd);
}

////////////////////////////
// Fullscreen transitions
////////////////////////////

static Vec4<f32> ft_start_colour;
static Vec4<f32> ft_end_colour;
static Vec4<f32> ft_current_colour;
static f32 ft_duration;
static f32 ft_current_t;
static Ichiaji::FullscreenTransitionCompleteCallback *ft_callback;
static uptr ft_callback_data;
static void update_fullscreen_transition() {
    static bool clear_next_frame = false;

    if (ft_duration == 0.0f) return;

    if (clear_next_frame) {
        clear_next_frame = false;
        ft_duration = 0.0f;

        if (ft_callback) ft_callback(ft_callback_data);

        return;
    }

    if (ft_current_t == ft_duration) {
        clear_next_frame = true;
    }

    ft_current_colour = {
        ichigo_lerp(ft_start_colour.r, safe_ratio_1(ft_current_t, ft_duration), ft_end_colour.r),
        ichigo_lerp(ft_start_colour.g, safe_ratio_1(ft_current_t, ft_duration), ft_end_colour.g),
        ichigo_lerp(ft_start_colour.b, safe_ratio_1(ft_current_t, ft_duration), ft_end_colour.b),
        ichigo_lerp(ft_start_colour.a, safe_ratio_1(ft_current_t, ft_duration), ft_end_colour.a),
    };

    ft_current_t = clamp(ft_current_t + Ichigo::Internal::dt, 0.0f, ft_duration);
}

void Ichiaji::fullscreen_transition(Vec4<f32> from, Vec4<f32> to, f32 t, FullscreenTransitionCompleteCallback *on_complete, uptr callback_data) {
    ft_start_colour    = from;
    ft_end_colour      = to;
    ft_current_colour  = from;
    ft_duration        = t;
    ft_current_t       = 0.0f;
    ft_callback        = on_complete;
    ft_callback_data   = callback_data;
}

////////////////////////////
// END Fullscreen transitions
////////////////////////////

static void enter_game_state([[maybe_unused]] uptr data) {
    Ichiaji::fullscreen_transition({0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, 0.3f, nullptr, 0);
    init_game();
    Ichiaji::program_state = Ichiaji::PS_GAME;
}

static void enter_main_menu_state([[maybe_unused]] uptr data) {
    deinit_game();
    Ichiaji::program_state = Ichiaji::PS_MAIN_MENU;
}

static void enter_new_program_state(Ichiaji::ProgramState state) {
    switch (state) {
        case Ichiaji::PS_MAIN_MENU: {
            Ichiaji::fullscreen_transition({0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 0.3f, enter_main_menu_state, 0);
        } break;

        case Ichiaji::PS_GAME: {
            Ichiaji::fullscreen_transition({0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 0.3f, enter_game_state, 0);
        } break;

        default: break;
    }
}

// Runs at the beginning of a new frame
void Ichigo::Game::frame_begin() {
#ifdef ICHIGO_DEBUG
    Ichigo::Internal::dt *= DEBUG_timescale;
#endif

    // Fullscreen transition
    update_fullscreen_transition();
}

#ifdef ICHIGO_DEBUG
static constexpr f32 NOCLIP_SPEED = 5.0f;
static void DEBUG_noclip_update(Ichigo::Entity *e) {
    f32 actual_noclip_speed = NOCLIP_SPEED;

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_SPACE].down) {
        actual_noclip_speed *= 3.0f;
    }

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT].down) {
        e->col.pos.x -= actual_noclip_speed * Ichigo::Internal::dt;
    }

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_RIGHT].down) {
        e->col.pos.x += actual_noclip_speed * Ichigo::Internal::dt;
    }

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_UP].down) {
        e->col.pos.y -= actual_noclip_speed * Ichigo::Internal::dt;
    }

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_DOWN].down) {
        e->col.pos.y += actual_noclip_speed * Ichigo::Internal::dt;
    }
}
#endif

// Runs right before the engine begins to render
void Ichigo::Game::update_and_render() {
    if (Ichiaji::all_levels[Ichiaji::current_level_id].update_proc) Ichiaji::all_levels[Ichiaji::current_level_id].update_proc();

    if (Ichigo::Internal::keyboard_state[IK_F5].down_this_frame) {
        if (current_language == ENGLISH) current_language = JAPANESE;
        else                             current_language = ENGLISH;

        Ichigo::show_info(TL_STR(SWITCH_LANGUAGE));
    }

    if (!controller_connected && Internal::gamepad.connected) {
        show_info(TL_STR(CONTROLLER_CONNECTED));
        controller_connected = true;
    } else if (controller_connected && !Internal::gamepad.connected) {
        show_info(TL_STR(CONTROLLER_DISCONNECTED));
        controller_connected = false;
    }

    switch (Ichiaji::program_state) {
        case Ichiaji::PS_MAIN_MENU: {
#define FADE_IN_DURATION 1.5f
#define MENU_ITEM_COUNT 2
            static u32 selected_menu_item = 0;
            static f32 fade_in_t = -1.0f;

            if (fade_in_t < 0.0f) {
                fade_in_t = FADE_IN_DURATION;
            }

            Ichigo::DrawCommand info_text_cmd = {
                .type              = TEXT,
                .coordinate_system = CAMERA,
                .transform         = m4identity_f32,
                .string            = TL_STR(INFO_TEXT),
                .string_length     = std::strlen(TL_STR(INFO_TEXT)),
                .string_pos        = {Ichigo::Camera::screen_tile_dimensions.x, Ichigo::Camera::screen_tile_dimensions.y - 0.1f},
                .text_style        = info_style
            };

            Ichigo::push_draw_command(info_text_cmd);

            Ichigo::DrawCommand credit_text_cmd = {
                .type              = TEXT,
                .coordinate_system = CAMERA,
                .transform         = m4identity_f32,
                .string            = TL_STR(CREDIT_TEXT),
                .string_length     = std::strlen(TL_STR(CREDIT_TEXT)),
                .string_pos        = {0.0f, Ichigo::Camera::screen_tile_dimensions.y - 1.05f},
                .text_style        = credit_style
            };

            Ichigo::push_draw_command(credit_text_cmd);

            bool menu_down_button_down_this_frame   = Ichigo::Internal::keyboard_state[Ichigo::IK_DOWN].down_this_frame || Ichigo::Internal::gamepad.down.down_this_frame;
            bool menu_up_button_down_this_frame     = Ichigo::Internal::keyboard_state[Ichigo::IK_UP].down_this_frame || Ichigo::Internal::gamepad.up.down_this_frame;
            bool menu_select_button_down_this_frame = Ichigo::Internal::keyboard_state[IK_ENTER].down_this_frame || Ichigo::Internal::gamepad.a.down_this_frame || Ichigo::Internal::gamepad.start.down_this_frame;

            if (menu_down_button_down_this_frame) {
                selected_menu_item = (selected_menu_item + 1) % MENU_ITEM_COUNT;
            } else if (menu_up_button_down_this_frame) {
                selected_menu_item = DEC_POSITIVE_OR(selected_menu_item, MENU_ITEM_COUNT - 1);
            }

            Ichigo::DrawCommand title_draw_cmd = {
                .type              = TEXT,
                .coordinate_system = CAMERA,
                .transform         = m4identity_f32,
                .string            = TL_STR(TITLE),
                .string_length     = std::strlen(TL_STR(TITLE)),
                .string_pos        = {Ichigo::Camera::screen_tile_dimensions.x / 2.0f, Ichigo::Camera::screen_tile_dimensions.y / ichigo_lerp(6.0f, fade_in_t / FADE_IN_DURATION, 4.0f)},
                .text_style        = title_style
            };

            Ichigo::push_draw_command(title_draw_cmd);

            if (menu_select_button_down_this_frame) {
                if (selected_menu_item == 0) {
                    enter_new_program_state(Ichiaji::PS_GAME);
                } else if (selected_menu_item == 1) {
                    std::exit(0);
                }
            }

            if (fade_in_t > 0.0f) {
                Ichigo::DrawCommand fade_in_cmd = {
                    .type              = SOLID_COLOUR_RECT,
                    .coordinate_system = SCREEN,
                    .transform         = m4identity_f32,
                    .rect              = {{0.0f, 0.0f}, 1.0f, 1.0f},
                    .colour            = {0.0f, 0.0f, 0.0f, fade_in_t / FADE_IN_DURATION}
                };

                Ichigo::push_draw_command(fade_in_cmd);

                fade_in_t -= Ichigo::Internal::dt;

                if (fade_in_t < 0.0f) {
                    fade_in_t = 0.0f;
                }
            } else {
                f64 t = std::sin(Internal::platform_get_current_time() * 2.0f);
                t *= t;
                t = 0.5f + 0.5f * t;
                Vec4<f32> pulse_colour = {0.6f, 0.2f, ichigo_lerp(0.4f, t, 0.9f), 1.0f};

                Ichigo::DrawCommand menu_draw_cmd = {
                    .type              = TEXT,
                    .coordinate_system = CAMERA,
                    .transform         = m4identity_f32,
                    .string            = TL_STR(START_GAME),
                    .string_length     = std::strlen(TL_STR(START_GAME)),
                    .string_pos        = {Ichigo::Camera::screen_tile_dimensions.x / 2.0f, Ichigo::Camera::screen_tile_dimensions.y / 1.5f},
                    .text_style        = menu_item_style
                };

                if (selected_menu_item == 0) menu_draw_cmd.text_style.colour = pulse_colour;
                else                         menu_draw_cmd.text_style.colour = colour_white;


                Ichigo::push_draw_command(menu_draw_cmd);

                if (selected_menu_item == 1) menu_draw_cmd.text_style.colour = pulse_colour;
                else                         menu_draw_cmd.text_style.colour = colour_white;

                menu_draw_cmd.string        = TL_STR(EXIT);
                menu_draw_cmd.string_length = std::strlen(TL_STR(EXIT)),
                menu_draw_cmd.string_pos    = {Ichigo::Camera::screen_tile_dimensions.x / 2.0f, (Ichigo::Camera::screen_tile_dimensions.y / 1.5f) + 1.0f};

                Ichigo::push_draw_command(menu_draw_cmd);
            }
#undef FADE_IN_DURATION
#undef MENU_ITEM_COUNT
        } break;

        case Ichiaji::PS_GAME: {
            // == Cheats ==
#ifdef ICHIGO_DEBUG
            static bool DEBUG_noclip_enabled = false;
            static Ichigo::EntityUpdateProc *DEBUG_old_proc = nullptr;
            if (Internal::keyboard_state[IK_LEFT_CONTROL].down && Internal::keyboard_state[IK_N].down_this_frame) {
                if (!DEBUG_noclip_enabled) {
                    Ichigo::show_info("CHEAT: Noclip enabled.");
                    auto *player = Ichigo::get_entity(Ichiaji::player_entity_id);

                    if (player) {
                        DEBUG_old_proc       = player->update_proc;
                        player->update_proc  = DEBUG_noclip_update;
                        DEBUG_noclip_enabled = true;
                    }
                } else {
                    Ichigo::show_info("CHEAT: Noclip disabled.");
                    auto *player = Ichigo::get_entity(Ichiaji::player_entity_id);

                    if (player) {
                        player->update_proc  = DEBUG_old_proc;
                        DEBUG_noclip_enabled = false;
                    }
                }
            }

            if (Internal::keyboard_state[IK_LEFT_CONTROL].down && Internal::keyboard_state[IK_1].down_this_frame) {
                Ichigo::show_info("CHEAT: Load level ID 1.");
                change_level(1, false);
            }

            if (Internal::keyboard_state[IK_LEFT_CONTROL].down && Internal::keyboard_state[IK_2].down_this_frame) {
                Ichigo::show_info("CHEAT: Load level ID 2.");
                change_level(2, false);
            }

            if (Internal::keyboard_state[IK_LEFT_CONTROL].down && Internal::keyboard_state[IK_3].down_this_frame) {
                Ichigo::show_info("CHEAT: Load level ID 3.");
                change_level(3, false);
            }

            if (Internal::keyboard_state[IK_LEFT_CONTROL].down && Internal::keyboard_state[IK_M].down_this_frame) {
                Ichigo::show_info("CHEAT: Give money.");
                Ichiaji::current_save_data.player_data.money = 9999;
            }

            if (Internal::keyboard_state[IK_LEFT_CONTROL].down && Internal::keyboard_state[IK_DOWN].down_this_frame) {
                DEBUG_timescale = clamp(DEBUG_timescale - 0.10f, 0.1f, 2.0f);
                Bana::String s = make_string(64, Ichigo::Internal::temp_allocator);
                Bana::string_format(s, "Timescale: %.2f", DEBUG_timescale);
                Ichigo::show_info(s.data, s.length);
            }

            if (Internal::keyboard_state[IK_LEFT_CONTROL].down && Internal::keyboard_state[IK_UP].down_this_frame) {
                DEBUG_timescale = clamp(DEBUG_timescale + 0.10f, 0.1f, 2.0f);
                Bana::String s = make_string(64, Ichigo::Internal::temp_allocator);
                Bana::string_format(s, "Timescale: %.2f", DEBUG_timescale);
                Ichigo::show_info(s.data, s.length);
            }

            if (Internal::keyboard_state[IK_LEFT_CONTROL].down && (Internal::keyboard_state[IK_RIGHT].down_this_frame || Internal::keyboard_state[IK_LEFT].down_this_frame)) {
                DEBUG_timescale = 1.0f;
                Ichigo::show_info("(Reset) Timescale: 1.00");
            }
#endif

            draw_game_ui();

            if (Internal::keyboard_state[IK_ESCAPE].down_this_frame || Internal::gamepad.start.down_this_frame) {
                Ichiaji::program_state = Ichiaji::PS_PAUSE;
            }
        } break;

        case Ichiaji::PS_UI_MENU: {
            draw_game_ui();
            Ui::render_and_update_current_menu();
        } break;

        case Ichiaji::PS_PAUSE: {
            draw_game_ui();

#define PAUSE_MENU_FADE_DURATION 0.2f
#define PAUSE_MENU_ITEM_COUNT 4
            static u32 selected_menu_item = 0;
            static f32 fade_t = -1.0f;

            if (fade_t < 0.0f) {
                fade_t = PAUSE_MENU_FADE_DURATION;
            }

            bool menu_down_button_down_this_frame   = Ichigo::Internal::keyboard_state[Ichigo::IK_DOWN].down_this_frame || Ichigo::Internal::gamepad.down.down_this_frame;
            bool menu_up_button_down_this_frame     = Ichigo::Internal::keyboard_state[Ichigo::IK_UP].down_this_frame || Ichigo::Internal::gamepad.up.down_this_frame;
            bool menu_select_button_down_this_frame = Ichigo::Internal::keyboard_state[IK_ENTER].down_this_frame || Ichigo::Internal::gamepad.a.down_this_frame || Ichigo::Internal::gamepad.start.down_this_frame;

            if (menu_down_button_down_this_frame) {
                selected_menu_item = (selected_menu_item + 1) % PAUSE_MENU_ITEM_COUNT;
            } else if (menu_up_button_down_this_frame) {
                selected_menu_item = DEC_POSITIVE_OR(selected_menu_item, PAUSE_MENU_ITEM_COUNT - 1);
            }

            Ichigo::DrawCommand fade_in_cmd = {
                .type              = SOLID_COLOUR_RECT,
                .coordinate_system = SCREEN,
                .transform         = m4identity_f32,
                .rect              = {{0.0f, 0.0f}, 1.0f, 1.0f},
                .colour            = {0.0f, 0.0f, 0.0f, ichigo_lerp(0.75f, fade_t / PAUSE_MENU_FADE_DURATION, 0.0f)}
            };

            Ichigo::push_draw_command(fade_in_cmd);

            Ichigo::DrawCommand pause_text_cmd = {
                .type              = TEXT,
                .coordinate_system = CAMERA,
                .transform         = m4identity_f32,
                .string            = TL_STR(PAUSE),
                .string_length     = std::strlen(TL_STR(PAUSE)),
                .string_pos        = {Ichigo::Camera::screen_tile_dimensions.x / 2.0f, Ichigo::Camera::screen_tile_dimensions.y / ichigo_lerp(6.0f, fade_t / PAUSE_MENU_FADE_DURATION, 5.5f)},
                .text_style        = title_style
            };

            pause_text_cmd.text_style.colour = {1.0f, 1.0f, 1.0f, ichigo_lerp(1.0f, fade_t / PAUSE_MENU_FADE_DURATION, 0.0f)};

            Ichigo::push_draw_command(pause_text_cmd);

            f64 t = std::sin(Internal::platform_get_current_time() * 2.0f);
            t *= t;
            t = 0.5f + 0.5f * t;
            Vec4<f32> pulse_colour = {0.6f, 0.2f, ichigo_lerp(0.4f, t, 0.9f), 1.0f};

            Ichigo::DrawCommand menu_draw_cmd = {
                .type              = TEXT,
                .coordinate_system = CAMERA,
                .transform         = m4identity_f32,
                .string            = TL_STR(RESUME_GAME),
                .string_length     = std::strlen(TL_STR(RESUME_GAME)),
                .string_pos        = {Ichigo::Camera::screen_tile_dimensions.x / 2.0f, Ichigo::Camera::screen_tile_dimensions.y / 1.7f},
                .text_style        = menu_item_style
            };

            if (selected_menu_item == 0) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = colour_white;


            Ichigo::push_draw_command(menu_draw_cmd);

            if (selected_menu_item == 1) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = colour_white;

            menu_draw_cmd.string        = TL_STR(SAVE_GAME);
            menu_draw_cmd.string_length = std::strlen(TL_STR(SAVE_GAME)),
            menu_draw_cmd.string_pos    = {Ichigo::Camera::screen_tile_dimensions.x / 2.0f, (Ichigo::Camera::screen_tile_dimensions.y / 1.7f) + 1.0f};

            Ichigo::push_draw_command(menu_draw_cmd);

            if (selected_menu_item == 2) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = colour_white;

            menu_draw_cmd.string        = TL_STR(RETURN_TO_MENU);
            menu_draw_cmd.string_length = std::strlen(TL_STR(RETURN_TO_MENU)),
            menu_draw_cmd.string_pos    = {Ichigo::Camera::screen_tile_dimensions.x / 2.0f, (Ichigo::Camera::screen_tile_dimensions.y / 1.7f) + 2.0f};

            Ichigo::push_draw_command(menu_draw_cmd);

            if (selected_menu_item == 3) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = colour_white;

            menu_draw_cmd.string        = TL_STR(EXIT);
            menu_draw_cmd.string_length = std::strlen(TL_STR(EXIT)),
            menu_draw_cmd.string_pos    = {Ichigo::Camera::screen_tile_dimensions.x / 2.0f, (Ichigo::Camera::screen_tile_dimensions.y / 1.7f) + 3.0f};

            Ichigo::push_draw_command(menu_draw_cmd);

            fade_t -= Ichigo::Internal::dt;

            if (fade_t < 0.0f) {
                fade_t = 0.0f;
            }

            if (menu_select_button_down_this_frame) {
                if (selected_menu_item == 0) {
                    Ichiaji::program_state = Ichiaji::PS_GAME;
                    selected_menu_item = 0;
                    fade_t = -1.0f;
                } else if (selected_menu_item == 1) {
                    // TODO: Remove this option from the menu in the finished game!
                    if (Ichiaji::save_game()) {
                        Ichigo::show_info("Saved!");
                    } else {
                        Ichigo::show_info("Failed to save game.");
                    }

                    Ichiaji::program_state = Ichiaji::PS_GAME;
                    selected_menu_item = 0;
                    fade_t = -1.0f;
                } else if (selected_menu_item == 2) {
                    selected_menu_item = 0;
                    // fade_t = -1.0f;
                    enter_new_program_state(Ichiaji::PS_MAIN_MENU);
                } else if (selected_menu_item == 3) {
                    std::exit(0);
                }
            }
#undef PAUSE_MENU_FADE_DURATION
#undef PAUSE_MENU_ITEM_COUNT
        } break;

        default: break;
    }

    // Render fullscreen transition rect, if it exists
    if (ft_duration != 0.0f) {
        Ichigo::DrawCommand cmd = {
            .type              = SOLID_COLOUR_RECT,
            .coordinate_system = SCREEN,
            .transform         = m4identity_f32,
            .rect              = {{0.0f, 0.0f}, 1.0f, 1.0f},
            .colour            = ft_current_colour
        };

        Ichigo::push_draw_command(cmd);
    }
}

// Runs at the end of the fame (Thinking about this interface still, maybe we don't need these?)
void Ichigo::Game::frame_end() {
}

Bana::Array<Ichigo::EntityDescriptor> *Ichigo::Game::level_entity_descriptors() {
    return &current_entity_descriptors;
}

void Ichigo::Game::hard_reset_level() {
    Ichigo::show_info("Hard reset");

    Ichigo::kill_all_entities();
    respawn_all_entities(current_entity_descriptors, false);
}
