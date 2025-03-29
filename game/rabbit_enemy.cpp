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
    RS_HURT
};

struct Rabbit {
    RabbitState state;
    f32 cooldown_t;
    f32 health;
};

static const Ichigo::Animation rabbit_idle = {
    .tag                 = RS_IDLE,
    .cell_of_first_frame = 2 * 5,
    .cell_of_last_frame  = 2 * 5 + 3,
    .cell_of_loop_start  = 2 * 5,
    .cell_of_loop_end    = 2 * 5 + 3,
    .seconds_per_frame   = 0.08f
};

static const Ichigo::Animation rabbit_walk = {
    .tag                 = RS_WALK,
    .cell_of_first_frame = 0 * 5,
    .cell_of_last_frame  = 0 * 5 + 3,
    .cell_of_loop_start  = 0 * 5,
    .cell_of_loop_end    = 0 * 5 + 3,
    .seconds_per_frame   = 0.08f
};

static const Ichigo::Animation rabbit_hurt = {
    .tag                 = RS_HURT,
    .cell_of_first_frame = 1 * 5,
    .cell_of_last_frame  = 1 * 5 + 1,
    .cell_of_loop_start  = 1 * 5,
    .cell_of_loop_end    = 1 * 5 + 1,
    .seconds_per_frame   = 0.08f
};

static Ichigo::Sprite rabbit_sprite;
static Bana::BucketArray<Rabbit> rabbit_data;

#define RABBIT_SPRITE_CELL_WIDTH_PX  25
#define RABBIT_SPRITE_CELL_HEIGHT_PX 40
void RabbitEnemy::init() {
    rabbit_data   = make_bucket_array<Rabbit>(128, Ichigo::Internal::perm_allocator);
    rabbit_sprite = {};
    rabbit_sprite.pos_offset = {0.0f, 0.0f};
    rabbit_sprite.width      = pixels_to_metres(RABBIT_SPRITE_CELL_WIDTH_PX);
    rabbit_sprite.height     = pixels_to_metres(RABBIT_SPRITE_CELL_HEIGHT_PX);
    rabbit_sprite.sheet      = { RABBIT_SPRITE_CELL_WIDTH_PX, RABBIT_SPRITE_CELL_HEIGHT_PX, Assets::rabbit_texture_id };
    rabbit_sprite.animation  = {};
}

#define RABBIT_WANDER_STILL_TIME_MIN 1.0f
#define RABBIT_WANDER_STILL_TIME_MAX 3.0f
#define RABBIT_WANDER_WALK_TIME_MIN 0.3f
#define RABBIT_WANDER_WALK_TIME_MAX 0.6f
#define RABBIT_JUMP_CHARGE_TIME 0.4f
#define RABBIT_CHASE_JUMP_FREQ 0.5f
#define RABBIT_ALERT_DISTANCE_SQ 6.0f * 6.0f
#define RABBIT_ACTIVATE_DISTANCE_SQ 13.0f * 13.0f

static inline void maybe_enter_animation(Ichigo::Entity *entity, Ichigo::Animation animation) {
    if (entity->sprite.animation.tag != animation.tag) {
        entity->sprite.animation                    = animation;
        entity->sprite.current_animation_frame      = 0;
        entity->sprite.elapsed_animation_frame_time = 0.0f;
    }
}

void update(Ichigo::Entity *self) {
    if (Ichiaji::program_state != Ichiaji::PS_GAME) {
        return;
    }

    Rabbit &rabbit     = rabbit_data[BIT_CAST(Bana::BucketLocator, self->user_data_i64)];
    auto *player       = Ichigo::get_entity(Ichiaji::player_entity_id);
    Vec2<f32> distance = self->col.pos - player->col.pos;

    switch (rabbit.state) {
        case RS_IDLE: {
            if (distance.lengthsq() < RABBIT_ACTIVATE_DISTANCE_SQ) {
                rabbit.state      = RS_WANDER;
                rabbit.cooldown_t = rand_range_f32(RABBIT_WANDER_STILL_TIME_MIN, RABBIT_WANDER_STILL_TIME_MAX);
            }
        } break;

        case RS_WANDER: {
            maybe_enter_animation(self, rabbit_idle);

            if (distance.lengthsq() < RABBIT_ALERT_DISTANCE_SQ) {
                // TODO: Play a sound effect here.
                rabbit.state     = RS_ALERT;
                self->velocity.y = -3.0f;
            } else if (distance.lengthsq() > RABBIT_ACTIVATE_DISTANCE_SQ) {
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
            maybe_enter_animation(self, rabbit_walk);

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
            maybe_enter_animation(self, rabbit_idle);

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
            maybe_enter_animation(self, rabbit_idle);

            if (FLAG_IS_SET(self->flags, Ichigo::EF_ON_GROUND)) {
                rabbit.state      = RS_CHASE;
                rabbit.cooldown_t = RABBIT_CHASE_JUMP_FREQ;
            }
        } break;

        case RS_CHASE: {
            maybe_enter_animation(self, rabbit_walk);

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

        case RS_HURT: {
            maybe_enter_animation(self, rabbit_hurt);

            rabbit.cooldown_t -= Ichigo::Internal::dt;
            if (rabbit.cooldown_t <= 0.0f) {
                rabbit.state = RS_CHASE;
            }
        } break;
    }

    Ichigo::move_entity_in_world(self);
}

#define SPELL_DAMAGE 1.0f
#define FIRE_SPELL_DAMAGE 2.0f
void on_collide(Ichigo::Entity *self, Ichigo::Entity *other, [[maybe_unused]] Vec2<f32> normal, [[maybe_unused]] Vec2<f32> collision_normal, [[maybe_unused]] Vec2<f32> collision_pos) {
    if (other->user_type_id == ET_SPELL || other->user_type_id == ET_FIRE_SPELL) {
        Rabbit &rabbit = rabbit_data[BIT_CAST(Bana::BucketLocator, self->user_data_i64)];

        if (rabbit.state == RS_HURT) return;

        rabbit.state       = RS_HURT;
        rabbit.cooldown_t  = 1.0f;
        self->acceleration = {};
        rabbit.health -= (other->user_type_id == ET_SPELL ? SPELL_DAMAGE : FIRE_SPELL_DAMAGE) + Ichiaji::player_bonuses.attack_power;
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
    e->col            = {descriptor.pos, pixels_to_metres(RABBIT_SPRITE_CELL_WIDTH_PX), pixels_to_metres(RABBIT_SPRITE_CELL_HEIGHT_PX)};
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
