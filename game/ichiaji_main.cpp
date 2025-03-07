/*
    Ichigo demo game code.

    Game init and logic.

    Author: Braeden Hong
    Date:   2024/11/28
*/

#include "../ichigo.hpp"
#include "ichiaji.hpp"
#include "strings.hpp"
#include "irisu.hpp"
#include "moving_platform.hpp"
#include "particle_source.hpp"
#include "entrances.hpp"
#include "rabbit_enemy.hpp"

EMBED("assets/enemy.png", enemy_png)
EMBED("assets/coin.png", coin_spritesheet_png)
EMBED("assets/bg.png", test_bg)
EMBED("assets/music/song.mp3", test_song)
EMBED("assets/music/coin.mp3", coin_sound_mp3)

// Real tiles
EMBED("assets/overworld_tiles.png", tileset_png)

// Levels
#include "levels/level0.ichigolvl"
#include "levels/level1.ichigolvl"
#include "levels/cave_entrance.ichigolvl"
#include "levels/first.ichigolvl"

Ichiaji::Level Ichiaji::all_levels[] = {
    Level0::level,
    Level1::level,
    CaveEntranceLevel::level,
    FirstLevel::level,
};

// UI
EMBED("assets/coin-collected.png", ui_collected_coin_png)
EMBED("assets/coin-uncollected.png", ui_uncollected_coin_png)
EMBED("assets/coin-ui-bg.png", ui_coin_background_png)
EMBED("assets/a-button.png", ui_a_button_png)
EMBED("assets/enter-key.png", ui_enter_key_png)

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

static Ichigo::TextureID tileset_texture    = 0;
static Ichigo::TextureID enemy_texture_id   = 0;
static Ichigo::TextureID coin_texture_id    = 0;
static Ichigo::TextureID test_bg_texture_id = 0;
static Ichigo::AudioID   test_music_id      = 0;
static Ichigo::AudioID   coin_sound         = 0;

static Ichigo::TextureID ui_collected_coin_texture   = 0;
static Ichigo::TextureID ui_uncollected_coin_texture = 0;
static Ichigo::TextureID ui_coin_background          = 0;
static Ichigo::TextureID ui_a_button                 = 0;
static Ichigo::TextureID ui_enter_key                = 0;

static f32 ui_coin_background_width_in_metres  = 0.0f;
static f32 ui_coin_background_height_in_metres = 0.0f;

static Ichigo::SpriteSheet tileset_sheet;

static Ichigo::Mixer::PlayingAudioID game_bgm;

static Bana::Array<Ichigo::EntityDescriptor> current_entity_descriptors{};

struct Coin {
    bool collected;
    Ichigo::EntityID id;
};

Language              current_language           = ENGLISH;
Ichiaji::GameSaveData Ichiaji::current_save_data = {};
Ichiaji::ProgramState Ichiaji::program_state     = Ichiaji::MAIN_MENU;
bool                  Ichiaji::input_disabled    = false;
i64                   Ichiaji::current_level_id  = 0;
Ichigo::EntityID      Ichiaji::player_entity_id  = NULL_ENTITY_ID;

static void gert_update(Ichigo::Entity *gert) {
    if (Ichiaji::program_state != Ichiaji::GAME) {
        return;
    }

    Ichigo::EntityControllers::patrol_controller(gert);
}

#define GERT_KILL_REWARD 10
static void gert_collide(Ichigo::Entity *gert, Ichigo::Entity *other, [[maybe_unused]] Vec2<f32> normal, [[maybe_unused]] Vec2<f32> collision_normal, [[maybe_unused]] Vec2<f32> collision_pos) {
    if (other->user_type_id == ET_SPELL) {
        // TODO: Show some coin particle effect when you kill enemies?
        Ichiaji::current_save_data.player_data.money += GERT_KILL_REWARD;
        Ichigo::kill_entity_deferred(gert->id);
    }
}

static Ichigo::EntityID gert_id;
static bool controller_connected = false;

static void spawn_gert(Vec2<f32> pos) {
    Ichigo::Entity *enemy = Ichigo::spawn_entity();
    gert_id = enemy->id;
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
    gert_sprite.sheet.texture     = enemy_texture_id;
    gert_sprite.sheet.cell_width  = 32;
    gert_sprite.sheet.cell_height = 32;
    gert_sprite.animation         = gert_idle;

    enemy->sprite = gert_sprite;
}

bool Ichiaji::save_game() {
    Ichigo::Internal::PlatformFile *save_file = Ichigo::Internal::platform_open_file_write(Bana::temp_string("./default.save"));

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
    ICHIGO_INFO("Loading save V%d\n", version_number);
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
    Irisu::init();
    MovingPlatform::init();
    ParticleSource::init();
    Entrances::init();
    RabbitEnemy::init();

    test_bg_texture_id    = Ichigo::load_texture(test_bg, test_bg_len);
    enemy_texture_id      = Ichigo::load_texture(enemy_png, enemy_png_len);
    coin_texture_id       = Ichigo::load_texture(coin_spritesheet_png, coin_spritesheet_png_len);
    tileset_texture       = Ichigo::load_texture(tileset_png, tileset_png_len);
    test_music_id         = Ichigo::load_audio(test_song, test_song_len);
    coin_sound            = Ichigo::load_audio(coin_sound_mp3, coin_sound_mp3_len);

    ui_collected_coin_texture   = Ichigo::load_texture(ui_collected_coin_png, ui_collected_coin_png_len);
    ui_uncollected_coin_texture = Ichigo::load_texture(ui_uncollected_coin_png, ui_uncollected_coin_png_len);
    ui_coin_background          = Ichigo::load_texture(ui_coin_background_png, ui_coin_background_png_len);
    ui_a_button                 = Ichigo::load_texture(ui_a_button_png, ui_a_button_png_len);
    ui_enter_key                = Ichigo::load_texture(ui_enter_key_png, ui_enter_key_png_len);

    tileset_sheet = { 32, 32, tileset_texture };

    // == Load levels ==
    for (u32 i = 0; i < ARRAY_LEN(Ichiaji::all_levels); ++i) {
        auto &level = Ichiaji::all_levels[i];

        if (level.init_proc) level.init_proc();

        // == Load level backgrounds ==
        for (u32 i = 0; i < level.background_descriptors.size; ++i) {
            auto &descriptor = level.background_descriptors[i];
            Ichigo::TextureID background_texture_id = Ichigo::load_texture(descriptor.png_data, descriptor.png_size);
            for (u32 j = 0; j < descriptor.backgrounds.size; ++j) {
                descriptor.backgrounds[j].texture_id = background_texture_id;
            }
        }
    }

    // Title screen level
    Ichiaji::current_level_id = 0;

    const Ichigo::Texture &t = Ichigo::Internal::textures[ui_coin_background];
    ui_coin_background_width_in_metres  = pixels_to_metres(t.width);
    ui_coin_background_height_in_metres = pixels_to_metres(t.height);

    Ichigo::Camera::mode = Ichigo::Camera::Mode::MANUAL;
    Ichigo::Camera::screen_tile_dimensions = {16.0f, 9.0f};
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
    for (u32 i = 0; i < level.background_descriptors.size; ++i) {
        const auto &descriptor = level.background_descriptors[i];
        for (u32 j = 0; j < descriptor.backgrounds.size && j < ICHIGO_MAX_BACKGROUNDS; ++j) {
            Ichigo::game_state.background_layers[j] = descriptor.backgrounds[j];
        }
    }

    // NOTE: "current_entity_descriptors" is copied to a Bana::Array because the editor may need to modify it.
    current_entity_descriptors.size = level.entity_descriptors.size;
    current_entity_descriptors.ensure_capacity(level.entity_descriptors.size);
    std::memcpy(current_entity_descriptors.data, level.entity_descriptors.data, level.entity_descriptors.size * sizeof(Ichigo::EntityDescriptor));

    // == Spawn entities in world ==
    respawn_all_entities(current_entity_descriptors, first_start);
}

void Ichiaji::try_change_level(i64 level_id) {
    if (level_id >= (i64) ARRAY_LEN(all_levels)) {
        ICHIGO_ERROR("try_change_level: Invalid level index.");
        return;
    }

    change_level(level_id, false);
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

    // == Setup initial game state ==
    game_bgm = Ichigo::Mixer::play_audio(test_music_id, 1.0f, 1.0f, 1.0f, 0.864f, 54.188f);
}

static void deinit_game() {
    Ichigo::Camera::mode = Ichigo::Camera::Mode::MANUAL;
    Ichigo::Mixer::cancel_audio(game_bgm);

    change_level(0, false);
}

static void draw_game_ui() {
    static const Ichigo::TextStyle ui_style = {
        .alignment    = Ichigo::TextAlignment::CENTER,
        .scale        = 1.0f,
        .colour       = colour_white,
        .line_spacing = 100.0f
    };

    static constexpr Rect<f32> health_ui_rect  = {{0.2f, 0.2f}, 3.0f, 1.3f};
    static constexpr Vec2<f32> health_text_pos = {health_ui_rect.pos.x + health_ui_rect.w / 2.0f, health_ui_rect.pos.y + health_ui_rect.h / 2.0f};
    static Ichigo::DrawCommand health_text_background_cmd = {
        .type              = Ichigo::DrawCommandType::SOLID_COLOUR_RECT,
        .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
        .transform         = m4identity_f32,
        .rect              = health_ui_rect,
        .colour            = {0.0f, 0.0f, 0.0f, 0.75f}
    };

    Ichigo::push_draw_command(health_text_background_cmd);

    Bana::String health_string = Bana::make_string(64, Ichigo::Internal::temp_allocator);
    Bana::string_format(health_string, "%s %.1f", TL_STR(HEALTH_UI), Ichiaji::current_save_data.player_data.health);

    Bana::String money_string = Bana::make_string(64, Ichigo::Internal::temp_allocator);
    Bana::string_format(money_string, "%s %u", TL_STR(MONEY_UI), Ichiaji::current_save_data.player_data.money);

    Ichigo::DrawCommand health_text_cmd = {
        .type              = Ichigo::DrawCommandType::TEXT,
        .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
        .transform         = m4identity_f32,
        .string            = health_string.data,
        .string_length     = health_string.length,
        .string_pos        = health_text_pos,
        .text_style        = ui_style
    };

    Ichigo::DrawCommand money_text_cmd = {
        .type              = Ichigo::DrawCommandType::TEXT,
        .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
        .transform         = m4identity_f32,
        .string            = money_string.data,
        .string_length     = money_string.length,
        .string_pos        = health_text_pos + Vec2<f32>{0.0f, 0.5f},
        .text_style        = ui_style
    };

    Ichigo::push_draw_command(health_text_cmd);
    Ichigo::push_draw_command(money_text_cmd);
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
    Ichiaji::program_state = Ichiaji::GAME;
}

static void enter_main_menu_state([[maybe_unused]] uptr data) {
    deinit_game();
    Ichiaji::program_state = Ichiaji::MAIN_MENU;
}

static void enter_new_program_state(Ichiaji::ProgramState state) {
    switch (state) {
        case Ichiaji::MAIN_MENU: {
            Ichiaji::fullscreen_transition({0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 0.3f, enter_main_menu_state, 0);
        } break;

        case Ichiaji::GAME: {
            Ichiaji::fullscreen_transition({0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 0.3f, enter_game_state, 0);
        } break;

        case Ichiaji::PAUSE: {

        } break;
    }
}

// Runs at the beginning of a new frame
void Ichigo::Game::frame_begin() {
    // Fullscreen transition
    update_fullscreen_transition();
}

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
        case Ichiaji::MAIN_MENU: {
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
                    enter_new_program_state(Ichiaji::GAME);
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

        case Ichiaji::GAME: {
            draw_game_ui();

            if (Internal::keyboard_state[IK_ESCAPE].down_this_frame || Internal::gamepad.start.down_this_frame) {
                Ichiaji::program_state = Ichiaji::PAUSE;
            }
        } break;

        case Ichiaji::PAUSE: {
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
                    Ichiaji::program_state = Ichiaji::GAME;
                    selected_menu_item = 0;
                    fade_t = -1.0f;
                } else if (selected_menu_item == 1) {
                    if (Ichiaji::save_game()) {
                        Ichigo::show_info("Saved!");
                    } else {
                        Ichigo::show_info("Failed to save game.");
                    }

                    Ichiaji::program_state = Ichiaji::GAME;
                    selected_menu_item = 0;
                    fade_t = -1.0f;
                } else if (selected_menu_item == 2) {
                    selected_menu_item = 0;
                    // fade_t = -1.0f;
                    enter_new_program_state(Ichiaji::MAIN_MENU);
                } else if (selected_menu_item == 3) {
                    std::exit(0);
                }
            }
#undef PAUSE_MENU_FADE_DURATION
#undef PAUSE_MENU_ITEM_COUNT
        } break;
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
