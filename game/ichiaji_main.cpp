#include "../ichigo.hpp"
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

static Ichigo::TextureID tileset_texture    = 0;
static Ichigo::TextureID enemy_texture_id   = 0;
static Ichigo::TextureID coin_texture_id    = 0;
static Ichigo::TextureID test_bg_texture_id = 0;
static Ichigo::AudioID   test_music_id      = 0;

static Ichigo::TextureID ui_collected_coin_texture   = 0;
static Ichigo::TextureID ui_uncollected_coin_texture = 0;
static Ichigo::TextureID ui_coin_background          = 0;

static f32 ui_coin_background_width_in_metres  = 0.0f;
static f32 ui_coin_background_height_in_metres = 0.0f;

struct Coin {
    bool collected;
    Ichigo::EntityID id;
};

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
    enemy->update_proc    = Ichigo::EntityControllers::patrol_controller;

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

    const Ichigo::Texture &t = Ichigo::Internal::textures.at(ui_coin_background);
    ui_coin_background_width_in_metres  = pixels_to_metres(t.width);
    ui_coin_background_height_in_metres = pixels_to_metres(t.height);

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

    Ichigo::Mixer::play_audio(test_music_id, 1.0f, 1.0f, 1.0f, 0.864f, 54.188f);
}

// Runs at the beginning of a new frame
void Ichigo::Game::frame_begin() {
}

// Runs right before the engine begins to render
void Ichigo::Game::update_and_render() {
    if (!controller_connected && Internal::gamepad.connected) {
        show_info("Controller connected!");
        controller_connected = true;
    } else if (controller_connected && !Internal::gamepad.connected) {
        show_info("Controller disconnected!");
        controller_connected = false;
    }

    Ichigo::TextStyle style;
    style.scale     = 0.8f;
    style.alignment = Ichigo::TextAlignment::LEFT;
    style.colour    = {0.0f, 0.0f, 0.0f, 1.0f};

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

    // == UI ==
    static Ichigo::DrawCommand coins_background_cmd = {
        .type = TEXTURED_RECT,
        .coordinate_system = CAMERA,
        .texture_rect = {{0.1f, 0.1f}, ui_coin_background_width_in_metres, ui_coin_background_height_in_metres},
        .texture_id = ui_coin_background
    };

    Ichigo::push_draw_command(coins_background_cmd);

    for (u32 i = 0; i < ARRAY_LEN(coins); ++i) {
        Ichigo::DrawCommand coin_cmd = {
            .type = TEXTURED_RECT,
            .coordinate_system = CAMERA,
            .texture_rect = {{0.25f + i + (i * 0.1f), 0.37f}, 1.0f, 1.0f},
            .texture_id = coins[i].collected ? ui_collected_coin_texture : ui_uncollected_coin_texture
        };

        Ichigo::push_draw_command(coin_cmd);
    }

    if (Internal::keyboard_state[IK_G].down_this_frame) {
        spawn_gert();
    }
}

// Runs at the end of the fame (Thinking about this interface still, maybe we don't need these?)
void Ichigo::Game::frame_end() {
}
