/*
    Ichigo demo game code.

    Game logic for coin entity.

    Author:      Braeden Hong
    Last Edited: 2024/11/25
*/

#include "coin.hpp"

EMBED("assets/coin.png", coin_spritesheet_png)

static Ichigo::TextureID coin_texture_id = 0;

static void on_collide(Ichigo::Entity *coin, Ichigo::Entity *other, Vec2<f32> normal, Vec2<f32> collision_pos) {
}

void Coin::init() {
    coin_texture_id = Ichigo::load_texture(coin_spritesheet_png, coin_spritesheet_png_len);
}

Ichigo::EntityID Coin::spawn(Vec2<f32> pos) {
    Ichigo::Entity *coin = Ichigo::spawn_entity();

    std::strcpy(coin->name, "coin");

    coin->col                                  = {pos, 0.5f, 0.5f};
    coin->sprite.width                         = 1.0f;
    coin->sprite.height                        = 1.0f;
    coin->sprite.sheet.texture                 = coin_texture_id;
    coin->sprite.sheet.cell_width              = 32;
    coin->sprite.sheet.cell_height             = 32;
    coin->sprite.animation.cell_of_first_frame = 0;
    coin->sprite.animation.cell_of_last_frame  = 7;
    coin->sprite.animation.cell_of_loop_start  = 0;
    coin->sprite.animation.cell_of_loop_end    = 7;
    coin->sprite.animation.seconds_per_frame   = 0.12f;

    return coin->id;
}
