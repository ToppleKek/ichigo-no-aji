#include "rabbit_enemy.hpp"
#include "ichiaji.hpp"

EMBED("assets/rabbit.png", rabbit_sheet_png)

enum RabbitState {
    RS_IDLE,
    RS_WANDER,
    RS_JUMP,
    RS_ALERT,
};

struct Rabbit {
    RabbitState state;
};

static Ichigo::TextureID rabbit_texture_id;
static Ichigo::Sprite rabbit_sprite;
static u32 tex_width_px;
static u32 tex_height_px;
static Bana::BucketArray<Rabbit> rabbit_data;

void RabbitEnemy::init() {
    rabbit_texture_id = Ichigo::load_texture(rabbit_sheet_png, rabbit_sheet_png_len);
    rabbit_data       = make_bucket_array<Rabbit>(128, Ichigo::Internal::perm_allocator);

    const Ichigo::Texture &tex = Ichigo::Internal::textures[rabbit_texture_id];

    tex_width_px  = tex.width;
    tex_height_px = tex.height;

    rabbit_sprite = {};
    rabbit_sprite.pos_offset = {0.0f, 0.0f};
    rabbit_sprite.width      = pixels_to_metres(tex_width_px);
    rabbit_sprite.height     = pixels_to_metres(tex_height_px);
    rabbit_sprite.sheet      = { tex_width_px, tex_height_px, rabbit_texture_id };
    rabbit_sprite.animation  = {};
}

void update(Ichigo::Entity *self) {
    Rabbit &rabbit = rabbit_data[BIT_CAST(Bana::BucketLocator, self->user_data_i64)];
    auto *player   = Ichigo::get_entity(Ichiaji::player_entity_id);
    Vec2<f32> distance = self->col.pos - player->col.pos;

    switch (rabbit.state) {
        case RS_IDLE: {
            if (distance.length() < 2.0f) {
                rabbit.state     = RS_ALERT;
                self->velocity.y = -3.0f;
            }
        } break;

        case RS_WANDER: {

        } break;

        case RS_JUMP: {

        } break;

        case RS_ALERT: {
            if (distance.length() > 6.0f) {
                rabbit.state = RS_IDLE;
            }
        } break;
    }

    Ichigo::move_entity_in_world(self);
}

void on_collide(Ichigo::Entity *self, Ichigo::Entity *other, Vec2<f32> normal, [[maybe_unused]] Vec2<f32> collision_normal, [[maybe_unused]] Vec2<f32> collision_pos) {

}

void on_kill(Ichigo::Entity *self) {
    Bana::BucketLocator bl = BIT_CAST(Bana::BucketLocator, self->user_data_i64);
    rabbit_data.remove(bl);
}

void RabbitEnemy::spawn(const Ichigo::EntityDescriptor &descriptor) {
    Ichigo::Entity *e = Ichigo::spawn_entity();
    Ichigo::change_entity_draw_layer(e, 8);

    Rabbit r = { RS_IDLE };
    auto bl  = rabbit_data.insert(r);

    std::strcpy(e->name, "rabbit");
    e->col            = {descriptor.pos, pixels_to_metres(tex_width_px), pixels_to_metres(tex_height_px)};
    e->max_velocity   = {6.0f, 12.0f};
    e->movement_speed = 14.0f;
    e->gravity        = 9.8f;
    e->update_proc    = update;
    e->collide_proc   = on_collide;
    e->kill_proc      = on_kill;
    e->user_type_id   = ET_RABBIT;
    e->sprite         = rabbit_sprite;
    e->user_data_i64  = BIT_CAST(i64, bl);
}
