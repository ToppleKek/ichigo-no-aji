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

Ichiaji::ProgramState Ichiaji::program_state = Ichiaji::MAIN_MENU;
Ichiaji::Level Ichiaji::current_level        = {};
Language current_language                    = ENGLISH;

static Coin coins[3] = {};

static void on_coin_collide(Ichigo::Entity *coin, Ichigo::Entity *other, [[maybe_unused]] Vec2<f32> normal, [[maybe_unused]] Vec2<f32> collision_normal, [[maybe_unused]] Vec2<f32> collision_pos) {
    if (std::strcmp(other->name, "player") == 0) {
        for (u32 i = 0; i < ARRAY_LEN(coins); ++i) {
            if (coin->id == coins[i].id) {
                coins[i].collected = true;
                Ichigo::Mixer::play_audio_oneshot(coin_sound, 1.0f, 1.0f, 1.0f);
                break;
            }
        }

        Ichigo::kill_entity_deferred(coin->id);
    }
}

static void gert_update(Ichigo::Entity *gert) {
    if (Ichiaji::program_state != Ichiaji::GAME) {
        return;
    }

    Ichigo::EntityControllers::patrol_controller(gert);
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

    Ichigo::Animation gert_idle   = {};
    gert_idle.cell_of_first_frame = 0;
    gert_idle.cell_of_last_frame  = 4;
    gert_idle.cell_of_loop_start  = 0;
    gert_idle.cell_of_loop_end    = 4;
    gert_idle.seconds_per_frame   = 0.08f;

    Ichigo::Sprite gert_sprite    = {};
    gert_sprite.width             = 1.0f;
    gert_sprite.height            = 1.0f;
    gert_sprite.pos_offset        = Util::calculate_centered_pos_offset(enemy->col, gert_sprite.width, gert_sprite.height);
    gert_sprite.sheet.texture     = enemy_texture_id;
    gert_sprite.sheet.cell_width  = 32;
    gert_sprite.sheet.cell_height = 32;
    gert_sprite.animation         = gert_idle;

    enemy->sprite = gert_sprite;
}

[[maybe_unused]] static Ichigo::EntityID spawn_coin(Vec2<f32> pos) {
    Ichigo::Entity *coin = Ichigo::spawn_entity();

    std::strcpy(coin->name, "coin");

    coin->col                                  = {pos, 1.0f, 1.0f};
    coin->sprite.width                         = 1.0f;
    coin->sprite.height                        = 1.0f;
    coin->sprite.sheet.texture                 = coin_texture_id;
    coin->collide_proc                         = on_coin_collide;
    coin->sprite.sheet.cell_width              = 32;
    coin->sprite.sheet.cell_height             = 32;
    coin->sprite.animation.cell_of_first_frame = 0;
    coin->sprite.animation.cell_of_last_frame  = 7;
    coin->sprite.animation.cell_of_loop_start  = 0;
    coin->sprite.animation.cell_of_loop_end    = 7;
    coin->sprite.animation.seconds_per_frame   = 0.12f;

    return coin->id;
}

static Ichigo::EntityID spawn_entrance(Vec2<f32> pos, i64 entrance_id) {
    Ichigo::Entity *entrance = Ichigo::spawn_entity();

    std::strcpy(entrance->name, "entr");

    entrance->col                                  = {pos, 1.0f, 1.0f};
    entrance->sprite.width                         = 1.0f;
    entrance->sprite.height                        = 1.0f;
    entrance->sprite.sheet.texture                 = coin_texture_id;
    entrance->sprite.sheet.cell_width              = 32;
    entrance->sprite.sheet.cell_height             = 32;
    entrance->sprite.animation.cell_of_first_frame = 0;
    entrance->sprite.animation.cell_of_last_frame  = 7;
    entrance->sprite.animation.cell_of_loop_start  = 0;
    entrance->sprite.animation.cell_of_loop_end    = 7;
    entrance->sprite.animation.seconds_per_frame   = 0.12f;
    entrance->user_data                            = entrance_id;

    return entrance->id;
}

void Ichigo::Game::init() {
    // == Load assets ==
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

    // == Setup level entrance maps ==
    for (u32 i = 0; i < Level1::level.entity_descriptors.size; ++i) {
        const auto &descriptor = Level1::level.entity_descriptors[i];
        if (descriptor.type == Ichiaji::ET_ENTRANCE) {
            Level1::setup_entrance(descriptor.data);
        }
    }

    // == Load level backgrounds ==
    for (u32 i = 0; i < Level1::level.background_descriptors.size; ++i) {
        auto &descriptor = Level1::level.background_descriptors[i];
        Ichigo::TextureID background_texture_id = Ichigo::load_texture(descriptor.png_data, descriptor.png_size);
        for (u32 j = 0; j < descriptor.backgrounds.size; ++j) {
            descriptor.backgrounds[j].texture_id = background_texture_id;
        }
    }

    // Title screen level
    Ichiaji::current_level = Level0::level;

    const Ichigo::Texture &t = Ichigo::Internal::textures[ui_coin_background];
    ui_coin_background_width_in_metres  = pixels_to_metres(t.width);
    ui_coin_background_height_in_metres = pixels_to_metres(t.height);

    Ichigo::Camera::mode = Ichigo::Camera::Mode::MANUAL;
    Ichigo::Camera::screen_tile_dimensions = {16.0f, 9.0f};
}

static void respawn_all_entities(const Bana::Array<Ichigo::EntityDescriptor> &descriptors) {
    for (u32 i = 0; i < descriptors.size; ++i) {
        const auto &d = descriptors[i];

        switch (d.type) {
            case Ichiaji::ET_PLAYER: {
                Ichigo::Entity *player = Ichigo::spawn_entity();
                Irisu::init(player, d.pos);
                Ichigo::Camera::mode = Ichigo::Camera::Mode::FOLLOW;
                Ichigo::Camera::follow(player->id);
            } break;

            case Ichiaji::ET_GERT: {
                spawn_gert(d.pos);
            } break;

            case Ichiaji::ET_COIN: {

            } break;

            case Ichiaji::ET_ENTRANCE: {
                spawn_entrance(d.pos, d.data);
            } break;

            default: {
                ICHIGO_ERROR("Invalid/unknown entity type: %d", d.type);
            }
        }
    }
}

static void change_level(Ichiaji::Level level) {
    Ichigo::kill_all_entities();
    std::memset(&Ichigo::game_state.background_layers, 0, sizeof(Ichigo::game_state.background_layers));

    Ichiaji::current_level = level;
    Ichigo::set_tilemap(Ichiaji::current_level.tilemap_data, tileset_sheet);

    // == Setup backgrounds ==
    Ichigo::game_state.background_colour = level.background_colour;
    for (u32 i = 0; i < level.background_descriptors.size; ++i) {
        const auto &descriptor = level.background_descriptors[i];
        for (u32 j = 0; j < descriptor.backgrounds.size && j < ICHIGO_MAX_BACKGROUNDS; ++j) {
            Ichigo::game_state.background_layers[j] = descriptor.backgrounds[j];
        }
    }

    current_entity_descriptors.size = Ichiaji::current_level.entity_descriptors.size;
    current_entity_descriptors.ensure_capacity(Ichiaji::current_level.entity_descriptors.size);
    std::memcpy(current_entity_descriptors.data, Ichiaji::current_level.entity_descriptors.data, Ichiaji::current_level.entity_descriptors.size * sizeof(Ichigo::EntityDescriptor));

    // == Spawn entities in world ==
    respawn_all_entities(current_entity_descriptors);
}

static void init_game() {
    // == SETUP CURRENT LEVEL ==
    change_level(Level1::level);

    // coins[0].id = spawn_coin({42.0f, 4.0f});
    // coins[1].id = spawn_coin({42.0f, 14.0f});
    // coins[2].id = spawn_coin({52.0f, 4.0f});


    // == Setup initial game state ==
    game_bgm = Ichigo::Mixer::play_audio(test_music_id, 1.0f, 1.0f, 1.0f, 0.864f, 54.188f);
}

static void deinit_game() {
    Ichigo::Camera::mode = Ichigo::Camera::Mode::MANUAL;
    Ichigo::Mixer::cancel_audio(game_bgm);

    std::memset(coins, 0, sizeof(coins));

    change_level(Level0::level);
}

static void draw_game_ui() {
    static Ichigo::DrawCommand coins_background_cmd = {
        .type              = Ichigo::TEXTURED_RECT,
        .coordinate_system = Ichigo::CAMERA,
        .texture_rect      = {{0.1f, 0.1f}, ui_coin_background_width_in_metres, ui_coin_background_height_in_metres},
        .texture_id        = ui_coin_background
    };

    Ichigo::push_draw_command(coins_background_cmd);

    for (u32 i = 0; i < ARRAY_LEN(coins); ++i) {
        Ichigo::DrawCommand coin_cmd = {
            .type              = Ichigo::TEXTURED_RECT,
            .coordinate_system = Ichigo::CAMERA,
            .texture_rect      = {{0.25f + i + (i * 0.1f), 0.37f}, 1.0f, 1.0f},
            .texture_id        = coins[i].collected ? ui_collected_coin_texture : ui_uncollected_coin_texture
        };

        Ichigo::push_draw_command(coin_cmd);
    }
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
                .string            = TL_STR(INFO_TEXT),
                .string_length     = std::strlen(TL_STR(INFO_TEXT)),
                .string_pos        = {Ichigo::Camera::screen_tile_dimensions.x, Ichigo::Camera::screen_tile_dimensions.y - 0.1f},
                .text_style        = info_style
            };

            Ichigo::push_draw_command(info_text_cmd);

            Ichigo::DrawCommand credit_text_cmd = {
                .type              = TEXT,
                .coordinate_system = CAMERA,
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
#define PAUSE_MENU_ITEM_COUNT 3
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
                .rect              = {{0.0f, 0.0f}, 1.0f, 1.0f},
                .colour            = {0.0f, 0.0f, 0.0f, ichigo_lerp(0.75f, fade_t / PAUSE_MENU_FADE_DURATION, 0.0f)}
            };

            Ichigo::push_draw_command(fade_in_cmd);

            Ichigo::DrawCommand pause_text_cmd = {
                .type              = TEXT,
                .coordinate_system = CAMERA,
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
                .string            = TL_STR(RESUME_GAME),
                .string_length     = std::strlen(TL_STR(RESUME_GAME)),
                .string_pos        = {Ichigo::Camera::screen_tile_dimensions.x / 2.0f, Ichigo::Camera::screen_tile_dimensions.y / 1.5f},
                .text_style        = menu_item_style
            };

            if (selected_menu_item == 0) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = colour_white;


            Ichigo::push_draw_command(menu_draw_cmd);

            if (selected_menu_item == 1) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = colour_white;

            menu_draw_cmd.string        = TL_STR(RETURN_TO_MENU);
            menu_draw_cmd.string_length = std::strlen(TL_STR(RETURN_TO_MENU)),
            menu_draw_cmd.string_pos    = {Ichigo::Camera::screen_tile_dimensions.x / 2.0f, (Ichigo::Camera::screen_tile_dimensions.y / 1.5f) + 1.0f};

            Ichigo::push_draw_command(menu_draw_cmd);

            if (selected_menu_item == 2) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = colour_white;

            menu_draw_cmd.string        = TL_STR(EXIT);
            menu_draw_cmd.string_length = std::strlen(TL_STR(EXIT)),
            menu_draw_cmd.string_pos    = {Ichigo::Camera::screen_tile_dimensions.x / 2.0f, (Ichigo::Camera::screen_tile_dimensions.y / 1.5f) + 2.0f};

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
                    selected_menu_item = 0;
                    // fade_t = -1.0f;
                    enter_new_program_state(Ichiaji::MAIN_MENU);
                } else if (selected_menu_item == 2) {
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

void Ichigo::Game::set_level_entity_descriptors([[maybe_unused]] Bana::FixedArray<Ichigo::EntityDescriptor> descriptors) {
    // Bana::fixed_array_copy(Ichiaji::current_level.entity_descriptors, descriptors);
}

void Ichigo::Game::hard_reset_level() {
    Ichigo::show_info("Hard reset");

    Ichigo::kill_all_entities();
    respawn_all_entities(current_entity_descriptors);
}
