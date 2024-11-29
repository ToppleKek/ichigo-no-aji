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

// Real tiles
EMBED("assets/tiles.png", tileset_png)

// Tilemaps
EMBED("assets/lvl1.ichigotm", level1_tilemap)

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

static Ichigo::TextureID tileset_texture    = 0;
static Ichigo::TextureID enemy_texture_id   = 0;
static Ichigo::TextureID coin_texture_id    = 0;
static Ichigo::TextureID test_bg_texture_id = 0;
static Ichigo::AudioID   test_music_id      = 0;

static Ichigo::TextureID ui_collected_coin_texture   = 0;
static Ichigo::TextureID ui_uncollected_coin_texture = 0;
static Ichigo::TextureID ui_coin_background          = 0;
static Ichigo::TextureID ui_a_button                 = 0;
static Ichigo::TextureID ui_enter_key                = 0;

static f32 ui_coin_background_width_in_metres  = 0.0f;
static f32 ui_coin_background_height_in_metres = 0.0f;

static Ichigo::Mixer::PlayingAudioID game_bgm;

struct Coin {
    bool collected;
    Ichigo::EntityID id;
};

Ichiaji::ProgramState Ichiaji::program_state = Ichiaji::MAIN_MENU;
bool Ichiaji::modal_open                     = false;
Language current_language                    = ENGLISH;

static Coin coins[3] = {};

static void on_coin_collide(Ichigo::Entity *coin, Ichigo::Entity *other, [[maybe_unused]] Vec2<f32> normal, [[maybe_unused]] Vec2<f32> collision_pos) {
    if (std::strcmp(other->name, "player") == 0) {
        for (u32 i = 0; i < ARRAY_LEN(coins); ++i) {
            if (coin->id == coins[i].id) {
                coins[i].collected = true;
                break;
            }
        }

        Ichigo::kill_entity_deferred(coin->id);
    }
}

static void gert_update(Ichigo::Entity *gert) {
    if (Ichiaji::program_state != Ichiaji::GAME || Ichiaji::modal_open) {
        return;
    }

    Ichigo::EntityControllers::patrol_controller(gert);
}

static Ichigo::EntityID gert_id;
static bool controller_connected = false;

static void spawn_gert() {
    Ichigo::Entity *enemy = Ichigo::spawn_entity();
    gert_id = enemy->id;
    std::strcpy(enemy->name, "gert");
    enemy->col            = {{18.0f, 14.0f}, 0.5f, 0.5f};
    enemy->max_velocity   = {2.0f, 12.0f};
    enemy->movement_speed = 6.0f;
    enemy->gravity        = 9.8f;
    enemy->update_proc    = gert_update;

    Ichigo::Animation gert_idle   = {};
    gert_idle.cell_of_first_frame = 0;
    gert_idle.cell_of_last_frame  = 0;
    gert_idle.cell_of_loop_start  = 0;
    gert_idle.cell_of_loop_end    = 0;
    gert_idle.seconds_per_frame   = 0.0f;

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

static Ichigo::EntityID spawn_coin(Vec2<f32> pos) {
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

void Ichigo::Game::init() {
    // == Load assets ==
    test_bg_texture_id    = Ichigo::load_texture(test_bg, test_bg_len);
    enemy_texture_id      = Ichigo::load_texture(enemy_png, enemy_png_len);
    coin_texture_id       = Ichigo::load_texture(coin_spritesheet_png, coin_spritesheet_png_len);
    test_music_id         = Ichigo::load_audio(test_song, test_song_len);
    tileset_texture       = Ichigo::load_texture(tileset_png, tileset_png_len);

    ui_collected_coin_texture   = Ichigo::load_texture(ui_collected_coin_png, ui_collected_coin_png_len);
    ui_uncollected_coin_texture = Ichigo::load_texture(ui_uncollected_coin_png, ui_uncollected_coin_png_len);
    ui_coin_background          = Ichigo::load_texture(ui_coin_background_png, ui_coin_background_png_len);
    ui_a_button                 = Ichigo::load_texture(ui_a_button_png, ui_a_button_png_len);
    ui_enter_key                = Ichigo::load_texture(ui_enter_key_png, ui_enter_key_png_len);

    const Ichigo::Texture &t = Ichigo::Internal::textures.at(ui_coin_background);
    ui_coin_background_width_in_metres  = pixels_to_metres(t.width);
    ui_coin_background_height_in_metres = pixels_to_metres(t.height);

    Ichigo::Camera::mode = Ichigo::Camera::Mode::MANUAL;
}

static void init_game() {
    Ichigo::game_state.background_colour = {0.54f, 0.84f, 1.0f, 1.0f};
    Ichigo::game_state.background_layers[0].texture_id     = test_bg_texture_id;
    Ichigo::game_state.background_layers[0].flags          = Ichigo::BG_REPEAT_X;
    Ichigo::game_state.background_layers[0].start_position = {0.0f, 0.0f};
    Ichigo::game_state.background_layers[0].scroll_speed   = {0.5f, 0.6f};

    Ichigo::SpriteSheet tileset_sheet = {};
    tileset_sheet.cell_width  = 32;
    tileset_sheet.cell_height = 32;
    tileset_sheet.texture     = tileset_texture;

    Ichigo::set_tilemap((u8 *) level1_tilemap, tileset_sheet);

    // == Spawn entities in world ==
    Ichigo::Entity *player = Ichigo::spawn_entity();
    Irisu::init(player);

    coins[0].id = spawn_coin({42.0f, 4.0f});
    coins[1].id = spawn_coin({42.0f, 14.0f});
    coins[2].id = spawn_coin({52.0f, 4.0f});

    spawn_gert();

    // == Setup initial game state ==
    Ichigo::game_state.player_entity_id = player->id;

    Ichigo::Camera::mode = Ichigo::Camera::Mode::FOLLOW;
    Ichigo::Camera::follow(player->id);

    Ichigo::Mixer::master_volume = 0.4f;

    game_bgm = Ichigo::Mixer::play_audio(test_music_id, 1.0f, 1.0f, 1.0f, 0.864f, 54.188f);
}

static void deinit_game() {
    Ichigo::Camera::mode = Ichigo::Camera::Mode::MANUAL;
    Ichigo::Mixer::cancel_audio(game_bgm);

    std::memset(coins, 0, sizeof(coins));

    Irisu::deinit();

    Ichigo::kill_all_entities();
    Ichigo::game_state.background_colour    = {};
    Ichigo::game_state.background_layers[0] = {};

    Ichigo::set_tilemap(nullptr, {});
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

// Runs at the beginning of a new frame
void Ichigo::Game::frame_begin() {
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
                .string_pos        = {SCREEN_TILE_WIDTH, SCREEN_TILE_HEIGHT - 0.1f},
                .text_style        = info_style
            };

            Ichigo::push_draw_command(info_text_cmd);

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
                .string_pos        = {SCREEN_TILE_WIDTH / 2.0f, SCREEN_TILE_HEIGHT / ichigo_lerp(6.0f, fade_in_t / FADE_IN_DURATION, 4.0f)},
                .text_style        = title_style
            };

            Ichigo::push_draw_command(title_draw_cmd);

            if (menu_select_button_down_this_frame) {
                if (selected_menu_item == 0) {
                    init_game();
                    Ichiaji::program_state = Ichiaji::GAME;
                    Ichiaji::modal_open    = true;
                    fade_in_t = -1.0f;
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
                    .string_pos        = {SCREEN_TILE_WIDTH / 2.0f, SCREEN_TILE_HEIGHT / 1.5f},
                    .text_style        = menu_item_style
                };

                if (selected_menu_item == 0) menu_draw_cmd.text_style.colour = pulse_colour;
                else                         menu_draw_cmd.text_style.colour = colour_white;


                Ichigo::push_draw_command(menu_draw_cmd);

                if (selected_menu_item == 1) menu_draw_cmd.text_style.colour = pulse_colour;
                else                         menu_draw_cmd.text_style.colour = colour_white;

                menu_draw_cmd.string        = TL_STR(EXIT);
                menu_draw_cmd.string_length = std::strlen(TL_STR(EXIT)),
                menu_draw_cmd.string_pos    = {SCREEN_TILE_WIDTH / 2.0f, (SCREEN_TILE_HEIGHT / 1.5f) + 1.0f};

                Ichigo::push_draw_command(menu_draw_cmd);
            }
#undef FADE_IN_DURATION
#undef MENU_ITEM_COUNT
        } break;

        case Ichiaji::GAME: {
            // static Ichigo::TextStyle style = {
            //     .alignment = Ichigo::TextAlignment::LEFT,
            //     .scale     = 0.8f,
            //     .colour    = {0.0f, 0.0f, 0.0f, 1.0f}
            // };

            // const char *ichiaji = "Ichigo no Aji! いちごのあじ！ イチゴノアジ！ 苺の味！ Ｉｃｈｉｇｏ ｎｏ Ａｊｉ！";
            // Ichigo::DrawCommand test_draw_command2;
            // test_draw_command2.coordinate_system = Ichigo::CoordinateSystem::CAMERA;
            // test_draw_command2.type              = Ichigo::DrawCommandType::TEXT;
            // test_draw_command2.string            = ichiaji;
            // test_draw_command2.string_length     = std::strlen(ichiaji);
            // test_draw_command2.string_pos        = {0.0f, 1.0f};
            // test_draw_command2.text_style        = style;

            // Ichigo::push_draw_command(test_draw_command2);

            // const char *kanji_test = "一番、二番、三番、四番。「ラムネは子供っぽくないです。青春の飲み物ですから」世界一美味しい苺パスタを作るアイドル";
            // Ichigo::DrawCommand test_draw_command3;
            // style.alignment = Ichigo::TextAlignment::CENTER;
            // test_draw_command3.coordinate_system = Ichigo::CoordinateSystem::CAMERA;
            // test_draw_command3.type              = Ichigo::DrawCommandType::TEXT;
            // test_draw_command3.string            = kanji_test;
            // test_draw_command3.string_length     = std::strlen(kanji_test);
            // test_draw_command3.string_pos        = {8.0f, 3.0f};
            // test_draw_command3.text_style        = style;

            // Ichigo::push_draw_command(test_draw_command3);
            draw_game_ui();

            // Draw the "how to play" menu
            if (Ichiaji::modal_open) {
                static Ichigo::DrawCommand bg_cmd = {
                    .type              = SOLID_COLOUR_RECT,
                    .coordinate_system = SCREEN,
                    .rect              = {{0.0f, 0.0f}, 1.0f, 1.0f},
                    .colour            = {0.0f, 0.0f, 0.0f, 0.75f}
                };

                Ichigo::push_draw_command(bg_cmd);

                Ichigo::DrawCommand header_cmd = {
                    .type              = TEXT,
                    .coordinate_system = CAMERA,
                    .string            = TL_STR(HOW_TO_PLAY),
                    .string_length     = std::strlen(TL_STR(HOW_TO_PLAY)),
                    .string_pos        = {SCREEN_TILE_WIDTH / 2.0f, SCREEN_TILE_HEIGHT / 6.0f},
                    .text_style        = title_style
                };

                Ichigo::push_draw_command(header_cmd);

                const char *setsume_str = TL_STR(Internal::gamepad.connected ? EXPLANATION_CONTROLLER : EXPLANATION_KEYBOARD);
                Ichigo::DrawCommand setsume_text_cmd = {
                    .type              = TEXT,
                    .coordinate_system = CAMERA,
                    .string            = setsume_str,
                    .string_length     = std::strlen(setsume_str),
                    .string_pos        = {SCREEN_TILE_WIDTH / 2.0f, SCREEN_TILE_HEIGHT / 3.0f},
                    .text_style        = menu_item_style
                };

                Ichigo::push_draw_command(setsume_text_cmd);

                Ichigo::DrawCommand confirmation_text_cmd = {
                    .type              = TEXT,
                    .coordinate_system = CAMERA,
                    .string            = TL_STR(GOT_IT),
                    .string_length     = std::strlen(TL_STR(GOT_IT)),
                    .string_pos        = {SCREEN_TILE_WIDTH / 2.0f, SCREEN_TILE_HEIGHT - 1.0f},
                    .text_style        = menu_item_style
                };

                Ichigo::push_draw_command(confirmation_text_cmd);

                f32 got_it_text_width = Ichigo::get_text_width(TL_STR(GOT_IT), std::strlen(TL_STR(GOT_IT)), menu_item_style);
                Ichigo::DrawCommand button_cmd = {
                    .type              = TEXTURED_RECT,
                    .coordinate_system = CAMERA,
                    .texture_rect      = {{(SCREEN_TILE_WIDTH / 2.0f) + got_it_text_width * 0.6f, SCREEN_TILE_HEIGHT - 1.4f}, 0.5f, 0.5f},
                    .texture_id        = Internal::gamepad.connected ? ui_a_button : ui_enter_key
                };

                Ichigo::push_draw_command(button_cmd);

                bool accept_button_down_this_frame = Internal::keyboard_state[IK_ENTER].down_this_frame || Internal::gamepad.a.down_this_frame;

                if (accept_button_down_this_frame) {
                    Ichiaji::modal_open = false;
                }
            } else {
                if (Internal::keyboard_state[IK_G].down_this_frame) {
                    spawn_gert();
                }

                if (Internal::keyboard_state[IK_ESCAPE].down_this_frame || Internal::gamepad.start.down_this_frame) {
                    Ichiaji::program_state = Ichiaji::PAUSE;
                }
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
                .string_pos        = {SCREEN_TILE_WIDTH / 2.0f, SCREEN_TILE_HEIGHT / ichigo_lerp(6.0f, fade_t / PAUSE_MENU_FADE_DURATION, 5.5f)},
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
                .string_pos        = {SCREEN_TILE_WIDTH / 2.0f, SCREEN_TILE_HEIGHT / 1.5f},
                .text_style        = menu_item_style
            };

            if (selected_menu_item == 0) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = colour_white;


            Ichigo::push_draw_command(menu_draw_cmd);

            if (selected_menu_item == 1) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = colour_white;

            menu_draw_cmd.string        = TL_STR(RETURN_TO_MENU);
            menu_draw_cmd.string_length = std::strlen(TL_STR(RETURN_TO_MENU)),
            menu_draw_cmd.string_pos    = {SCREEN_TILE_WIDTH / 2.0f, (SCREEN_TILE_HEIGHT / 1.5f) + 1.0f};

            Ichigo::push_draw_command(menu_draw_cmd);

            if (selected_menu_item == 2) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = colour_white;

            menu_draw_cmd.string        = TL_STR(EXIT);
            menu_draw_cmd.string_length = std::strlen(TL_STR(EXIT)),
            menu_draw_cmd.string_pos    = {SCREEN_TILE_WIDTH / 2.0f, (SCREEN_TILE_HEIGHT / 1.5f) + 2.0f};

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
                    deinit_game();
                    selected_menu_item = 0;
                    Ichiaji::program_state = Ichiaji::MAIN_MENU;
                    fade_t = -1.0f;
                } else if (selected_menu_item == 2) {
                    std::exit(0);
                }
            }
#undef PAUSE_MENU_FADE_DURATION
#undef PAUSE_MENU_ITEM_COUNT
        } break;
    }
}

// Runs at the end of the fame (Thinking about this interface still, maybe we don't need these?)
void Ichigo::Game::frame_end() {
}
