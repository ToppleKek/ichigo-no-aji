#include "rabbit_enemy.hpp"
#include "ichiaji.hpp"
#include "asset_catalog.hpp"

#define RABBIT_BASE_HEALTH 3.0f

enum RabbitState {
    RS_IDLE,
    RS_WANDER,
    RS_WALK,
    RS_JUMP,
    RS_ALERT,
    RS_CHASE,
};

struct Rabbit {
    RabbitState state;
    f32 cooldown_t;
    f32 health;
};

static Ichigo::Sprite rabbit_sprite;
static u32 tex_width_px;
static u32 tex_height_px;
static Bana::BucketArray<Rabbit> rabbit_data;

void RabbitEnemy::init() {
    rabbit_data       = make_bucket_array<Rabbit>(128, Ichigo::Internal::perm_allocator);

    const Ichigo::Texture &tex = Ichigo::Internal::textures[Assets::rabbit_texture_id];

    tex_width_px  = tex.width;
    tex_height_px = tex.height;

    rabbit_sprite = {};
    rabbit_sprite.pos_offset = {0.0f, 0.0f};
    rabbit_sprite.width      = pixels_to_metres(tex_width_px);
    rabbit_sprite.height     = pixels_to_metres(tex_height_px);
    rabbit_sprite.sheet      = { tex_width_px, tex_height_px, Assets::rabbit_texture_id };
    rabbit_sprite.animation  = {};
}

#define RABBIT_WANDER_STILL_TIME_MIN 1.0f
#define RABBIT_WANDER_STILL_TIME_MAX 3.0f
#define RABBIT_WANDER_WALK_TIME_MIN 0.3f
#define RABBIT_WANDER_WALK_TIME_MAX 0.6f
#define RABBIT_JUMP_CHARGE_TIME 0.4f
#define RABBIT_CHASE_JUMP_FREQ 0.5f
void update(Ichigo::Entity *self) {
    if (Ichiaji::program_state != Ichiaji::PS_GAME) {
        return;
    }

    Rabbit &rabbit     = rabbit_data[BIT_CAST(Bana::BucketLocator, self->user_data_i64)];
    auto *player       = Ichigo::get_entity(Ichiaji::player_entity_id);
    Vec2<f32> distance = self->col.pos - player->col.pos;

    switch (rabbit.state) {
        case RS_IDLE: {
            if (distance.length() < 12.0f) {
                rabbit.state      = RS_WANDER;
                rabbit.cooldown_t = rand_range_f32(RABBIT_WANDER_STILL_TIME_MIN, RABBIT_WANDER_STILL_TIME_MAX);
            }
        } break;

        case RS_WANDER: {
            if (distance.length() < 2.0f) {
                // TODO: Play a sound effect here.
                rabbit.state     = RS_ALERT;
                self->velocity.y = -3.0f;
            } else if (distance.length() > 13.0f) {
                rabbit.state = RS_IDLE;
            } else {
                rabbit.cooldown_t -= Ichigo::Internal::dt;
                // FIXME: @fpsdep
                if (rabbit.cooldown_t <= 0.0f) {
                    rabbit.state      = RS_WALK;
                    rabbit.cooldown_t = rand_range_f32(RABBIT_WANDER_WALK_TIME_MIN, RABBIT_WANDER_WALK_TIME_MAX);

                    if (rand_01_f32() < 0.5f) SET_FLAG(self->flags, Ichigo::EF_FLIP_H);
                    else                      CLEAR_FLAG(self->flags, Ichigo::EF_FLIP_H);
                }
            }
        } break;

        case RS_WALK: {
            rabbit.cooldown_t -= Ichigo::Internal::dt;
            // FIXME: @fpsdep
            if (rabbit.cooldown_t <= 0.0f) {
                rabbit.state       = RS_WANDER;
                rabbit.cooldown_t  = rand_range_f32(RABBIT_WANDER_STILL_TIME_MIN, RABBIT_WANDER_STILL_TIME_MAX);
                self->acceleration = {};
            } else {
                f32 saved_speed = self->movement_speed;
                self->movement_speed /= 2.0f;
                Ichigo::EntityControllers::patrol_controller(self);
                self->movement_speed = saved_speed;

                // FIXME: We return here because the patrol controller also calls move_entity_in_world. Maybe we should just rewrite the patrol code here?
                return;
            }
        } break;

        case RS_JUMP: {
            rabbit.cooldown_t -= Ichigo::Internal::dt;
            if (rabbit.cooldown_t <= 0.0f) {
                // NOTE: We resue RS_ALERT because it just places us back in chase mode when we land on the ground.
                rabbit.state      = RS_ALERT;
                rabbit.cooldown_t = 0.0f;
                self->velocity.y  = -6.0f;
                self->velocity.x  = FLAG_IS_SET(self->flags, Ichigo::EF_FLIP_H) ? 4.0f : -4.0f;
                // TODO: Play a sound effect here.
            }
        } break;

        case RS_ALERT: {
            if (FLAG_IS_SET(self->flags, Ichigo::EF_ON_GROUND)) {
                rabbit.state      = RS_CHASE;
                rabbit.cooldown_t = RABBIT_CHASE_JUMP_FREQ;
            }
        } break;

        case RS_CHASE: {
            if (distance.length() > 8.0f) {
                rabbit.state       = RS_WANDER;
                self->acceleration = {};
            } else {
                rabbit.cooldown_t -= Ichigo::Internal::dt;

                if (rabbit.cooldown_t <= 0.0f) {
                    rabbit.cooldown_t = RABBIT_CHASE_JUMP_FREQ;

                    if (rand_01_f32() < 0.75f) {
                        rabbit.cooldown_t  = RABBIT_JUMP_CHARGE_TIME;
                        rabbit.state       = RS_JUMP;
                        self->acceleration = {};
                    }
                }

                self->acceleration.x = self->movement_speed * -signof(distance.x);

                if (self->acceleration.x > 0.0f) {
                    SET_FLAG(self->flags, Ichigo::EF_FLIP_H);
                } else {
                    CLEAR_FLAG(self->flags, Ichigo::EF_FLIP_H);
                }
            }
        } break;
    }

    Ichigo::move_entity_in_world(self);
}

#define SPELL_DAMAGE 1.0f
void on_collide(Ichigo::Entity *self, Ichigo::Entity *other, [[maybe_unused]] Vec2<f32> normal, [[maybe_unused]] Vec2<f32> collision_normal, [[maybe_unused]] Vec2<f32> collision_pos) {
    if (other->user_type_id == ET_SPELL) {
        Rabbit &rabbit = rabbit_data[BIT_CAST(Bana::BucketLocator, self->user_data_i64)];

        // TODO: Enter hurt state?
        rabbit.health -= SPELL_DAMAGE + Ichiaji::player_bonuses.attack_power;
        if (rabbit.health <= 0.0f) {
            // TODO @asset: Play a sound here.
            Ichiaji::drop_collectable(self->col.pos);
            Ichigo::kill_entity_deferred(self);
        }
    }
}

void on_kill(Ichigo::Entity *self) {
    Bana::BucketLocator bl = BIT_CAST(Bana::BucketLocator, self->user_data_i64);
    rabbit_data.remove(bl);
}

Ichigo::EntityID RabbitEnemy::spawn(const Ichigo::EntityDescriptor &descriptor) {
    Ichigo::Entity *e = Ichigo::spawn_entity();
    Ichigo::change_entity_draw_layer(e, 8);

    Rabbit r = {};
    r.state  = RS_IDLE;
    r.health = RABBIT_BASE_HEALTH;
    auto bl  = rabbit_data.insert(r);

    std::strcpy(e->name, "rabbit");
    e->col            = {descriptor.pos, pixels_to_metres(tex_width_px), pixels_to_metres(tex_height_px)};
    e->max_velocity   = {4.0f, 12.0f};
    e->movement_speed = 6.0f;
    e->gravity        = 9.8f;
    e->update_proc    = update;
    e->collide_proc   = on_collide;
    e->kill_proc      = on_kill;
    e->user_type_id   = ET_RABBIT;
    e->sprite         = rabbit_sprite;
    e->user_data_i64  = BIT_CAST(i64, bl);

    return e->id;
}
