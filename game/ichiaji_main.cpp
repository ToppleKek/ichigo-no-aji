#include "../ichigo.hpp"
#include "irisu.hpp"

EMBED("assets/enemy.png", enemy_png)
EMBED("assets/bg.png", test_bg)
EMBED("assets/music/song.mp3", test_song)

// Real tiles
EMBED("assets/tiles.png", tileset_png)

// Tilemaps
EMBED("assets/level1_flowers.ichigotm", level1_tilemap);

static Ichigo::TextureID tileset_texture = 0;
static Ichigo::TextureID enemy_texture_id   = 0;
static Ichigo::TextureID test_bg_texture_id = 0;
static Ichigo::AudioID   test_music_id      = 0;

static void entity_collide_proc(Ichigo::Entity *entity, Ichigo::Entity *other_entity) {
    ICHIGO_INFO("I (%s) just collided with %s!", entity->name, other_entity->name);
}

static Ichigo::EntityID gert_id;

void Ichigo::Game::init() {
    test_bg_texture_id    = Ichigo::load_texture(test_bg, test_bg_len);
    enemy_texture_id      = Ichigo::load_texture(enemy_png, enemy_png_len);
    test_music_id         = Ichigo::load_audio(test_song, test_song_len);
    tileset_texture       = Ichigo::load_texture(tileset_png, tileset_png_len);

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

    Ichigo::Entity *player = Ichigo::spawn_entity();
    Irisu::init(player);

    Ichigo::game_state.player_entity_id = player->id;

    Ichigo::Camera::mode = Ichigo::Camera::Mode::FOLLOW;
    Ichigo::Camera::follow(player->id);

    Ichigo::Entity *enemy = Ichigo::spawn_entity();
    gert_id = enemy->id;
    std::strcpy(enemy->name, "gert");
    enemy->col            = {{9.0f, 2.0f}, 0.5f, 0.5f};
    enemy->max_velocity   = {2.0f, 12.0f};
    enemy->movement_speed = 20.0f;
    enemy->gravity        = 9.8f;
    enemy->update_proc    = Ichigo::EntityControllers::patrol_controller;
    enemy->collide_proc   = entity_collide_proc;

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

    Ichigo::Mixer::play_audio(test_music_id, 1.0f, 1.0f, 1.0f, 0.864f, 54.188f);
}

void Ichigo::Game::frame_begin() {
    // Runs at the beginning of a new frame
}

void Ichigo::Game::update_and_render() {
    // Runs right before the engine begins to render

    Ichigo::DrawCommand test_draw_command;
    test_draw_command.coordinate_system = Ichigo::CoordinateSystem::WORLD;
    test_draw_command.type              = Ichigo::DrawCommandType::SOLID_COLOUR_RECT;
    test_draw_command.rect              = {{0.5f, 0.0f}, 0.1f, 0.1f};
    test_draw_command.colour            = {0.2f, 0.3f, 0.5f, 1.0f};
    Ichigo::push_draw_command(test_draw_command);

    Ichigo::TextStyle style;
    style.scale     = 0.8f;
    style.alignment = Ichigo::TextAlignment::LEFT;
    style.colour    = {0.0f, 0.0f, 0.0f, 1.0f};

    Ichigo::Entity *gert = Ichigo::get_entity(gert_id);
    if (gert) {
        Ichigo::DrawCommand test_draw_command2;
        test_draw_command2.coordinate_system = Ichigo::CoordinateSystem::WORLD;
        test_draw_command2.type              = Ichigo::DrawCommandType::TEXT;
        test_draw_command2.string            = "gert";
        test_draw_command2.string_length     = 4;
        test_draw_command2.string_pos        = gert->col.pos;
        test_draw_command2.text_style        = style;
        Ichigo::push_draw_command(test_draw_command2);
    }

    const char *ichiaji = "Ichigo no Aji! いちごのあじ！ イチゴノアジ！ 苺の味！ Ｉｃｈｉｇｏ ｎｏ Ａｊｉ！";
    Ichigo::DrawCommand test_draw_command2;
    test_draw_command2.coordinate_system = Ichigo::CoordinateSystem::CAMERA;
    test_draw_command2.type              = Ichigo::DrawCommandType::TEXT;
    test_draw_command2.string            = ichiaji;
    test_draw_command2.string_length     = std::strlen(ichiaji);
    test_draw_command2.string_pos        = {0.0f, 1.0f};
    test_draw_command2.text_style        = style;

    Ichigo::push_draw_command(test_draw_command2);

    const char *kanji_test = "一番、二番、三番、四番。「ラムネは子供っぽくないです。青春の飲み物ですから」世界一美味しい苺パスタを作るアイドル";
    Ichigo::DrawCommand test_draw_command3;
    style.alignment = Ichigo::TextAlignment::CENTER;
    test_draw_command3.coordinate_system = Ichigo::CoordinateSystem::CAMERA;
    test_draw_command3.type              = Ichigo::DrawCommandType::TEXT;
    test_draw_command3.string            = kanji_test;
    test_draw_command3.string_length     = std::strlen(kanji_test);
    test_draw_command3.string_pos        = {8.0f, 3.0f};
    test_draw_command3.text_style        = style;

    Ichigo::push_draw_command(test_draw_command3);

    Irisu::update();
}

void Ichigo::Game::frame_end() {
    // Runs at the end of the fame (Thinking about this interface still, maybe we don't need these?)
}
