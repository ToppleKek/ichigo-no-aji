#include "../ichigo.hpp"

EMBED("assets/test3.png", test_png_image)
EMBED("assets/grass.png", grass_tile_png)
static Ichigo::TextureID player_texture_id = 0;
static Ichigo::TextureID grass_texture_id  = 0;

#define SCREEN_TILE_WIDTH 16
#define SCREEN_TILE_HEIGHT 9

static u16 tilemap[SCREEN_TILE_HEIGHT][SCREEN_TILE_WIDTH] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0},
    {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1},
    {1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0},
    {1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
};

static Ichigo::TextureID tile_texture_map[2]{};

void Ichigo::Game::init() {
    player_texture_id = Ichigo::load_texture(test_png_image, test_png_image_len);
    grass_texture_id  = Ichigo::load_texture(grass_tile_png, grass_tile_png_len);

    tile_texture_map[1] = grass_texture_id;

    Ichigo::set_tilemap(SCREEN_TILE_WIDTH, SCREEN_TILE_HEIGHT, (u16 *) tilemap, tile_texture_map);
    Ichigo::player_entity = {
        {{3.0f, 5.0f}, 1.0f, 1.0f},
        {0.0f, 0.0f},
        {0.0f, 0.0f},
        player_texture_id,
        0,
        nullptr,
        Ichigo::EntityControllers::player_controller
    };

    // Ichigo::set_camera_mode(Ichigo::Camera::FOLLOW);
    // Ichigo::Camera::follow_camera_target = player.entity_id;
}

void Ichigo::Game::frame_begin() {
    // Ichigo::dt;
}

void Ichigo::Game::update_and_render() {


    // Ichigo::push_draw_command(...);
}

void Ichigo::Game::frame_end() {

}