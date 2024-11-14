#include "../ichigo.hpp"

EMBED("assets/test3.png", test_png_image)
EMBED("assets/grass.png", grass_tile_png)
EMBED("assets/other_tile.png", other_tile_png)
EMBED("assets/three_tile.png", three_tile_png)
EMBED("assets/enemy.png", enemy_png)
EMBED("assets/bg.png", test_bg)
EMBED("assets/music/yori.mp3", test_song)

static Ichigo::TextureID player_texture_id  = 0;
static Ichigo::TextureID enemy_texture_id   = 0;
static Ichigo::TextureID grass_texture_id   = 0;
static Ichigo::TextureID other_tile_texture_id   = 0;
static Ichigo::TextureID three_tile_texture_id   = 0;
static Ichigo::TextureID test_bg_texture_id = 0;
static Ichigo::AudioID   test_music_id      = 0;


#define TILEMAP_WIDTH SCREEN_TILE_WIDTH * 4
#define TILEMAP_HEIGHT SCREEN_TILE_HEIGHT * 2

static Ichigo::TileID tiles[TILEMAP_HEIGHT][TILEMAP_WIDTH] = {
    {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {2, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 3},
    {2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 3},
    {2, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 3},
    {2, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 3},
    {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 3},
    {2, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3},
    {2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
    {2, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 3},
    {2, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 3},
    {2, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3},
};

static Ichigo::TileInfo tile_info_map[4]{};

static Ichigo::Tilemap tilemap = {
    (Ichigo::TileID *) tiles,
    TILEMAP_WIDTH,
    TILEMAP_HEIGHT,
    tile_info_map,
    ARRAY_LEN(tile_info_map)
};

static void entity_collide_proc(Ichigo::Entity *entity, Ichigo::Entity *other_entity) {
    ICHIGO_INFO("I (%s) just collided with %s!", entity->name, other_entity->name);
}

static Ichigo::EntityID gert_id;

void Ichigo::Game::init() {
    test_bg_texture_id    = Ichigo::load_texture(test_bg, test_bg_len);
    player_texture_id     = Ichigo::load_texture(test_png_image, test_png_image_len);
    enemy_texture_id      = Ichigo::load_texture(enemy_png, enemy_png_len);
    grass_texture_id      = Ichigo::load_texture(grass_tile_png, grass_tile_png_len);
    other_tile_texture_id = Ichigo::load_texture(other_tile_png, other_tile_png_len);
    three_tile_texture_id = Ichigo::load_texture(three_tile_png, three_tile_png_len);
    test_music_id         = Ichigo::load_audio(test_song, test_song_len);

    Ichigo::game_state.background_colour = {0.54f, 0.84f, 1.0f, 1.0f};
    Ichigo::game_state.background_layers[0].texture_id     = test_bg_texture_id;
    Ichigo::game_state.background_layers[0].flags          = Ichigo::BG_REPEAT_X;
    Ichigo::game_state.background_layers[0].start_position = {0.0f, 0.0f};
    Ichigo::game_state.background_layers[0].scroll_speed   = {0.5f, 0.6f};

    std::strcpy(tile_info_map[0].name, "air");
    std::strcpy(tile_info_map[1].name, "grass");
    tile_info_map[1].texture_id = grass_texture_id;
    tile_info_map[1].friction   = 8.0f;
    SET_FLAG(tile_info_map[1].flags, TileFlag::TANGIBLE);

    std::strcpy(tile_info_map[2].name, "2tile");
    tile_info_map[2].texture_id = other_tile_texture_id;
    tile_info_map[2].friction   = 8.0f;
    SET_FLAG(tile_info_map[2].flags, TileFlag::TANGIBLE);

    std::strcpy(tile_info_map[3].name, "3tile");
    tile_info_map[3].texture_id = three_tile_texture_id;
    tile_info_map[3].friction   = 8.0f;
    SET_FLAG(tile_info_map[3].flags, TileFlag::TANGIBLE);

    Ichigo::set_tilemap(&tilemap);
    Ichigo::Entity *player = Ichigo::spawn_entity();

    std::strcpy(player->name, "player");
    player->col               = {{3.0f, 2.0f}, 0.5f, 1.5f};
    player->sprite_pos_offset = {-0.25f, -0.5f};
    player->sprite_w          = 1.0f;
    player->sprite_h          = 2.0f;
    player->max_velocity      = {8.0f, 12.0f};
    player->movement_speed    = 22.0f;
    player->jump_acceleration = 128.0f;
    player->friction          = 8.0f; // TODO: Friction should be a property of the ground maybe?
    player->gravity           = 12.0f; // TODO: gravity should be a property of the level?
    player->texture_id        = player_texture_id;
    player->update_proc       = Ichigo::EntityControllers::player_controller;
    player->collide_proc      = entity_collide_proc;

    Ichigo::game_state.player_entity_id = player->id;

    Ichigo::Camera::mode = Ichigo::Camera::Mode::FOLLOW;
    Ichigo::Camera::follow(player->id);

    Ichigo::Entity *enemy = Ichigo::spawn_entity();
    gert_id = enemy->id;
    std::strcpy(enemy->name, "gert");
    enemy->col            = {{9.0f, 2.0f}, 0.5f, 0.5f};
    enemy->sprite_w       = 0.5f;
    enemy->sprite_h       = 0.5f;
    enemy->max_velocity   = {2.0f, 12.0f};
    enemy->movement_speed = 4.0f;
    enemy->friction       = 8.0f;
    enemy->gravity        = 12.0f;
    enemy->texture_id     = enemy_texture_id;
    enemy->update_proc    = Ichigo::EntityControllers::patrol_controller;
    enemy->collide_proc   = entity_collide_proc;

    Ichigo::Mixer::play_audio(test_music_id, 1.0f, 1.0f, 1.0f);
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

    // ICHIGO_INFO("A button state: %d %d %d %d", Ichigo::Internal::gamepad.a.down, Ichigo::Internal::gamepad.a.up, Ichigo::Internal::gamepad.a.down_this_frame, Ichigo::Internal::gamepad.a.up_this_frame);
    // ICHIGO_INFO("LT: %f RT: %f", Ichigo::Internal::gamepad.lt, Ichigo::Internal::gamepad.rt);
    // ICHIGO_INFO("left stick: %f,%f", Ichigo::Internal::gamepad.stick_left.x, Ichigo::Internal::gamepad.stick_left.y);
}

void Ichigo::Game::frame_end() {
    // Runs at the end of the fame (Thinking about this interface still, maybe we don't need these?)
}
