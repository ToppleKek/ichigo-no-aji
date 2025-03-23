#include "collectables.hpp"
#include "asset_catalog.hpp"
#include "ichiaji.hpp"

#define COIN_LIFETIME 6.0f

static Ichigo::Animation coin_animation = {
    0,
    0,
    7,
    0,
    7,
    0.2f
};

static void coin_update(Ichigo::Entity *coin) {
    coin->user_data_f32_1 -= Ichigo::Internal::dt;
    if (coin->user_data_f32_1 <= 0.0f) {
        Ichigo::kill_entity_deferred(coin);
    } else if (coin->user_data_f32_1 <= 2.0f) {
        coin->user_data_f32_2 += Ichigo::Internal::dt;
        if (coin->user_data_f32_2 >= 0.05f) {
            coin->user_data_f32_2 = 0.0f;
            if (FLAG_IS_SET(coin->flags, Ichigo::EF_INVISIBLE)) CLEAR_FLAG(coin->flags, Ichigo::EF_INVISIBLE);
            else                                                SET_FLAG  (coin->flags, Ichigo::EF_INVISIBLE);
        }

    } else {
        Ichigo::move_entity_in_world(coin);
    }
}

void Collectables::spawn_coin(Vec2<f32> pos) {
    auto &tex = Ichigo::Internal::textures[Assets::coin_texture_id];
    Ichigo::Sprite coin_sprite = {
        {},
        pixels_to_metres(tex.width) / 8.0f,
        pixels_to_metres(tex.height),
        { tex.width / 8, tex.height, Assets::coin_texture_id },
        coin_animation,
        0,
        0.0f
    };

    auto *coin = Ichigo::spawn_entity();

    std::strcpy(coin->name, "coin");

    // TODO @robustness: This is here so that the coin doesn't get stuck in the ground when it spawns.
    pos.y -= 0.5f;

    coin->col                                  = {pos, coin_sprite.width, coin_sprite.height};
    coin->max_velocity                         = {9999.0f, 9999.0f};
    coin->velocity                             = {rand_range_f32(-1.0f, 1.0f), -4.0f};
    coin->gravity                              = 9.8f;
    coin->sprite                               = coin_sprite;
    coin->update_proc                          = coin_update;
    coin->user_type_id                         = ET_COIN;
    coin->user_data_f32_1                      = COIN_LIFETIME;
}

void Collectables::spawn_recovery_heart(Vec2<f32> pos) {
    auto &tex = Ichigo::Internal::textures[Assets::heart_texture_id];
    Ichigo::Sprite heart_sprite = {
        {},
        pixels_to_metres(tex.width),
        pixels_to_metres(tex.height),
        { tex.width, tex.height, Assets::heart_texture_id },
        {},
        0,
        0.0f
    };

    auto *heart = Ichigo::spawn_entity();

    std::strcpy(heart->name, "heart");

    // TODO @robustness: This is here so that the heart doesn't get stuck in the ground when it spawns.
    pos.y -= 0.5f;

    heart->col                                  = {pos, heart_sprite.width, heart_sprite.height};
    heart->max_velocity                         = {9999.0f, 9999.0f};
    heart->velocity                             = {rand_range_f32(-0.5f, 0.5f), -2.0f};
    heart->gravity                              = 5.0f;
    heart->sprite                               = heart_sprite;
    heart->update_proc                          = coin_update;
    heart->user_type_id                         = ET_RECOVERY_HEART;
    heart->user_data_f32_1                      = COIN_LIFETIME;
}

void Collectables::spawn_powerup(const Ichigo::EntityDescriptor &descriptor) {
    auto *e                  = Ichigo::spawn_entity();
    Ichigo::TextureID tex_id = 0;
    u32 animation_frames     = 0;

    if (descriptor.type == ET_HP_UP_COLLECTABLE) {
        std::strcpy(e->name, "hp_up");
        tex_id = Assets::hp_up_texture_id;
        animation_frames = 7;
    } else if (descriptor.type == ET_ATTACK_SPEED_UP_COLLECTABLE) {
        std::strcpy(e->name, "attack_speed_up");
        tex_id = Assets::attack_speed_up_texture_id;
        animation_frames = 7;
    } else {
        assert(false && "spawn_powerup called with an entity ID that is not a powerup!");
    }

    auto &tex = Ichigo::Internal::textures[tex_id];

    Ichigo::Sprite sprite = {
        {},
        pixels_to_metres(tex.width) / (f32) animation_frames,
        pixels_to_metres(tex.height),
        { tex.width / animation_frames, tex.height, tex_id },
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

    e->col           = {descriptor.pos, sprite.width, sprite.height};
    e->sprite        = sprite;
    e->user_type_id  = descriptor.type;
    e->user_data_i64 = descriptor.data;
}

void Collectables::spawn_fire_spell_collectable(const Ichigo::EntityDescriptor &descriptor) {
    static constexpr u32 animation_frames = 7;
    auto &tex = Ichigo::Internal::textures[Assets::fire_spell_collectable_texture_id];
    Ichigo::Sprite sprite = {
        {},
        pixels_to_metres(tex.width) / (f32) animation_frames,
        pixels_to_metres(tex.height),
        { tex.width / animation_frames, tex.height, Assets::fire_spell_collectable_texture_id },
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

    auto *e = Ichigo::spawn_entity();

    std::strcpy(e->name, "fire_spell_collectable");

    e->col          = {descriptor.pos, sprite.width, sprite.height};
    e->sprite       = sprite;
    e->user_type_id = ET_FIRE_SPELL_COLLECTABLE;
}
