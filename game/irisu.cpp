/*
    Ichigo demo game code.

    Game logic for player character.

    Author: Braeden Hong
    Date:   2024/11/23
*/

#include "irisu.hpp"

EMBED("assets/irisu.png", irisu_spritesheet_png)
EMBED("assets/music/boo_womp441.mp3", boo_womp_mp3)

enum IrisuState {
    IDLE,
    WALK,
    JUMP,
    FALL
};

static u32 irisu_state = IrisuState::IDLE;
static Ichigo::Animation irisu_idle = {};
static Ichigo::Animation irisu_walk = {};
static Ichigo::Animation irisu_jump = {};
static Ichigo::Animation irisu_fall = {};

static Ichigo::TextureID irisu_texture_id = 0;
static Ichigo::EntityID irisu_id = {};
static Ichigo::AudioID boo_womp_id = {};

static void on_collide(Ichigo::Entity *irisu, Ichigo::Entity *other, Vec2<f32> normal, f32 best_t) {
    ICHIGO_INFO("IRISU collide with %s: normal=%f,%f", other->name, normal.x, normal.y);
    if (normal.y == -1.0f) {
        ICHIGO_INFO("Bounce!");
        irisu->velocity.y -= 15.0f;
        Ichigo::kill_entity(other->id);
        Ichigo::Mixer::play_audio_oneshot(boo_womp_id, 1.0f, 1.0f, 1.0f);
    }
}

void Irisu::init(Ichigo::Entity *entity) {
    irisu_texture_id = Ichigo::load_texture(irisu_spritesheet_png, irisu_spritesheet_png_len);
    boo_womp_id = Ichigo::load_audio(boo_womp_mp3, boo_womp_mp3_len);
    irisu_id = entity->id;

    std::strcpy(entity->name, "player");
    entity->col               = {{6.0f, 2.0f}, 0.3f, 1.1f};
    entity->max_velocity      = {8.0f, 12.0f};
    entity->movement_speed    = 16.0f;
    entity->jump_acceleration = 4.5f;
    entity->gravity           = 14.0f; // TODO: gravity should be a property of the level?
    entity->update_proc       = Irisu::update;
    entity->collide_proc      = on_collide;

    irisu_idle.tag                 = IrisuState::IDLE;
    irisu_idle.cell_of_first_frame = 0;
    irisu_idle.cell_of_last_frame  = 7;
    irisu_idle.cell_of_loop_start  = 0;
    irisu_idle.cell_of_loop_end    = 7;
    irisu_idle.seconds_per_frame   = 0.08f;

    irisu_walk.tag                 = IrisuState::WALK;
    irisu_walk.cell_of_first_frame = 12;
    irisu_walk.cell_of_last_frame  = 16;
    irisu_walk.cell_of_loop_start  = 12;
    irisu_walk.cell_of_loop_end    = 16;
    irisu_walk.seconds_per_frame   = 0.1f;

    irisu_jump.tag                 = IrisuState::JUMP;
    irisu_jump.cell_of_first_frame = 24;
    irisu_jump.cell_of_last_frame  = 27;
    irisu_jump.cell_of_loop_start  = 25;
    irisu_jump.cell_of_loop_end    = 27;
    irisu_jump.seconds_per_frame   = 0.08f;

    irisu_fall.tag                 = IrisuState::FALL;
    irisu_fall.cell_of_first_frame = 36;
    irisu_fall.cell_of_last_frame  = 39;
    irisu_fall.cell_of_loop_start  = 37;
    irisu_fall.cell_of_loop_end    = 39;
    irisu_fall.seconds_per_frame   = 0.08f;

    Ichigo::Sprite irisu_sprite    = {};
    irisu_sprite.width             = pixels_to_metres(40.0f);
    irisu_sprite.height            = pixels_to_metres(40.0f);
    irisu_sprite.pos_offset        = Util::calculate_centered_pos_offset(entity->col, irisu_sprite.width, irisu_sprite.height);
    irisu_sprite.sheet.texture     = irisu_texture_id;
    irisu_sprite.sheet.cell_width  = 40;
    irisu_sprite.sheet.cell_height = 40;
    irisu_sprite.animation         = irisu_idle;

    entity->sprite = irisu_sprite;
}

static inline void maybe_enter_animation(Ichigo::Entity *entity, Ichigo::Animation animation) {
    if (entity->sprite.animation.tag != animation.tag) {
        entity->sprite.animation = animation;
        entity->sprite.current_animation_frame = 0;
        entity->sprite.elapsed_animation_frame_time = 0.0f;
    }
}

#define IRISU_MAX_JUMP_T 0.35f
void Irisu::update(Ichigo::Entity *irisu) {
    static f32 applied_t = 0.0f;
    static bool released_jump = false;

    irisu->acceleration = {0.0f, 0.0f};
    if (Ichigo::Internal::keyboard_state[Ichigo::IK_RIGHT].down || Ichigo::Internal::gamepad.right.down) {
        irisu->acceleration.x = irisu->movement_speed;
        SET_FLAG(irisu->flags, Ichigo::EF_FLIP_H);
    }

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT].down || Ichigo::Internal::gamepad.left.down) {
        irisu->acceleration.x = -irisu->movement_speed;
        CLEAR_FLAG(irisu->flags, Ichigo::EF_FLIP_H);
    }

    bool jump_button_down_this_frame = Ichigo::Internal::keyboard_state[Ichigo::IK_SPACE].down_this_frame || Ichigo::Internal::gamepad.a.down_this_frame || Ichigo::Internal::gamepad.b.down_this_frame;
    bool jump_button_down            = Ichigo::Internal::keyboard_state[Ichigo::IK_SPACE].down || Ichigo::Internal::gamepad.a.down || Ichigo::Internal::gamepad.b.down;
    bool run_button_down             = Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT_SHIFT].down || Ichigo::Internal::gamepad.x.down || Ichigo::Internal::gamepad.y.down;

    switch (irisu_state) {
        case IDLE: {
            if      (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND)) irisu_state = JUMP;
            else if (irisu->velocity.x != 0.0f)                        irisu_state = WALK;
            maybe_enter_animation(irisu, irisu_idle);

            if (jump_button_down_this_frame) {
                released_jump     = false;
                applied_t         = 0.0f;
                irisu_state       = JUMP;
                irisu->velocity.y = -irisu->jump_acceleration;
                maybe_enter_animation(irisu, irisu_jump);
            }
        } break;

        case WALK: {
            if      (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND))           irisu_state = JUMP;
            else if (irisu->velocity.x == 0.0f && irisu->acceleration.x == 0.0f) irisu_state = IDLE;

            maybe_enter_animation(irisu, irisu_walk);

            if (jump_button_down_this_frame) {
                released_jump     = false;
                applied_t         = 0.0f;
                irisu_state       = JUMP;
                irisu->velocity.y = -irisu->jump_acceleration;
                maybe_enter_animation(irisu, irisu_jump);
            }

            if (run_button_down) {
                irisu->max_velocity.x = 8.0f;
                // irisu_walk.seconds_per_frame = 0.06f;
            } else {
                irisu->max_velocity.x = 4.0f;
                // irisu_walk.seconds_per_frame = 0.08f;
            }
        } break;

        case JUMP: {
            if      ( FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x == 0.0f) irisu_state = IDLE;
            else if ( FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x != 0.0f) irisu_state = WALK;
            else if (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.y > 0.0f)  irisu_state = FALL;

            maybe_enter_animation(irisu, irisu_jump);

            if (!jump_button_down && !released_jump) {
                released_jump = true;
            }

            if (jump_button_down && !released_jump && applied_t < IRISU_MAX_JUMP_T) {
                CLEAR_FLAG(irisu->flags, Ichigo::EntityFlag::EF_ON_GROUND);
                f32 effective_dt = MIN(Ichigo::Internal::dt, IRISU_MAX_JUMP_T - applied_t);
                irisu->velocity.y = -irisu->jump_acceleration;
                applied_t += effective_dt;
            }
        } break;

        case FALL: {
            if      (FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x == 0.0f) irisu_state = IDLE;
            else if (FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x != 0.0f) irisu_state = WALK;

            maybe_enter_animation(irisu, irisu_fall);
        } break;
    }

    Ichigo::EntityMoveResult move_result = Ichigo::move_entity_in_world(irisu);

    if (irisu->velocity.y == 0.0f) {
        released_jump = true;
    }

    if (move_result == Ichigo::HIT_WALL) {
        irisu_state = IDLE;
        maybe_enter_animation(irisu, irisu_idle);
    }
}
