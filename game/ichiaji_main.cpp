#include "../ichigo.hpp"

EMBED("assets/test3.png", test_png_image)
EMBED("assets/grass.png", grass_tile_png)
EMBED("assets/enemy.png", enemy_png)
static Ichigo::TextureID player_texture_id = 0;
static Ichigo::TextureID enemy_texture_id  = 0;
static Ichigo::TextureID grass_texture_id  = 0;

#define TILEMAP_WIDTH SCREEN_TILE_WIDTH * 2
#define TILEMAP_HEIGHT SCREEN_TILE_HEIGHT * 2

static u16 tilemap[TILEMAP_HEIGHT][TILEMAP_WIDTH] = {
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1},
    {1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
    {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1},
    {1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1},
    {1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
};

static Ichigo::TextureID tile_texture_map[2]{};

static void entity_collide_proc(Ichigo::Entity *entity, Ichigo::Entity *other_entity) {
    ICHIGO_INFO("I (%s) just collided with %s!", entity->name, other_entity->name);
}

void Ichigo::Game::init() {
    player_texture_id = Ichigo::load_texture(test_png_image, test_png_image_len);
    enemy_texture_id  = Ichigo::load_texture(enemy_png, enemy_png_len);
    grass_texture_id  = Ichigo::load_texture(grass_tile_png, grass_tile_png_len);

    tile_texture_map[1] = grass_texture_id;

    Ichigo::set_tilemap(TILEMAP_WIDTH, TILEMAP_HEIGHT, (u16 *) tilemap, tile_texture_map);
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
    Ichigo::Camera::follow(player->id);

    Ichigo::Entity *enemy = Ichigo::spawn_entity();

    std::strcpy(enemy->name, "enemy");
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
}

void Ichigo::Game::frame_begin() {
    // Runs at the beginning of a new frame
}

void Ichigo::Game::update_and_render() {
    // Runs right before the engine begins to render

    // Ichigo::push_draw_command(...);
}

void Ichigo::Game::frame_end() {
    // Runs at the end of the fame (Thinking about this interface still, maybe we don't need these?)
}
