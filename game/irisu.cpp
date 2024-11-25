/*
    Ichigo demo game code.

    Game logic for player character.

    Author: Braeden Hong
    Date:   2024/11/24
*/

#include "irisu.hpp"

EMBED("assets/irisu.png", irisu_spritesheet_png)
EMBED("assets/music/boo_womp441.mp3", boo_womp_mp3)

enum IrisuState {
    IDLE,
    WALK,
    JUMP,
    FALL,
    DIVE,
    LAY_DOWN,
    GET_UP_SLOW,
    DIVE_BOOST
};

static u32 irisu_state = IrisuState::IDLE;
static Ichigo::Animation irisu_idle = {};
static Ichigo::Animation irisu_walk = {};
static Ichigo::Animation irisu_jump = {};
static Ichigo::Animation irisu_fall = {};
static Ichigo::Animation irisu_dive = {};
static Ichigo::Animation irisu_lay_down = {};
static Ichigo::Animation irisu_get_up_slow = {};

static Ichigo::TextureID irisu_texture_id = 0;
static Ichigo::EntityID irisu_id = {};
static Ichigo::AudioID boo_womp_id = {};

static void on_collide(Ichigo::Entity *irisu, Ichigo::Entity *other, Vec2<f32> normal, Vec2<f32> collision_pos) {
    ICHIGO_INFO("IRISU collide with %s: normal=%f,%f pos=%f,%f", other->name, normal.x, normal.y, collision_pos.x, collision_pos.y);
    if (normal.y == -1.0f && std::strcmp(other->name, "gert") == 0) {
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
    entity->col               = {{8.0f, 14.0f}, 0.3f, 1.1f};
    entity->max_velocity      = {8.0f, 12.0f};
    entity->movement_speed    = 16.0f;
    entity->jump_acceleration = 4.5f;
    entity->gravity           = 14.0f; // TODO: gravity should be a property of the level?
    entity->update_proc       = Irisu::update;
    entity->collide_proc      = on_collide;

    irisu_idle.tag                 = IrisuState::IDLE;
    irisu_idle.cell_of_first_frame = 5 * 17;
    irisu_idle.cell_of_last_frame  = irisu_idle.cell_of_first_frame + 7;
    irisu_idle.cell_of_loop_start  = irisu_idle.cell_of_first_frame;
    irisu_idle.cell_of_loop_end    = irisu_idle.cell_of_last_frame;
    irisu_idle.seconds_per_frame   = 0.08f;

    irisu_walk.tag                 = IrisuState::WALK;
    irisu_walk.cell_of_first_frame = 4 * 17;
    irisu_walk.cell_of_last_frame  = irisu_walk.cell_of_first_frame + 4;
    irisu_walk.cell_of_loop_start  = irisu_walk.cell_of_first_frame;
    irisu_walk.cell_of_loop_end    = irisu_walk.cell_of_last_frame;
    irisu_walk.seconds_per_frame   = 0.1f;

    irisu_jump.tag                 = IrisuState::JUMP;
    irisu_jump.cell_of_first_frame = 3 * 17;
    irisu_jump.cell_of_last_frame  = irisu_jump.cell_of_first_frame + 3;
    irisu_jump.cell_of_loop_start  = irisu_jump.cell_of_first_frame + 1;
    irisu_jump.cell_of_loop_end    = irisu_jump.cell_of_last_frame;
    irisu_jump.seconds_per_frame   = 0.08f;

    irisu_fall.tag                 = IrisuState::FALL;
    irisu_fall.cell_of_first_frame = 2 * 17;
    irisu_fall.cell_of_last_frame  = irisu_fall.cell_of_first_frame + 3;
    irisu_fall.cell_of_loop_start  = irisu_fall.cell_of_first_frame + 1;
    irisu_fall.cell_of_loop_end    = irisu_fall.cell_of_last_frame;
    irisu_fall.seconds_per_frame   = 0.08f;

    irisu_dive.tag                 = IrisuState::DIVE;
    irisu_dive.cell_of_first_frame = 0 * 17;
    irisu_dive.cell_of_last_frame  = irisu_dive.cell_of_first_frame + 7;
    irisu_dive.cell_of_loop_start  = irisu_dive.cell_of_first_frame + 6;
    irisu_dive.cell_of_loop_end    = irisu_dive.cell_of_last_frame;
    irisu_dive.seconds_per_frame   = 0.08f;

    irisu_lay_down.tag                 = IrisuState::LAY_DOWN;
    // The "lay down" cell is one frame after the end of the dive animation.
    irisu_lay_down.cell_of_first_frame = irisu_dive.cell_of_last_frame + 1;
    irisu_lay_down.cell_of_last_frame  = irisu_dive.cell_of_last_frame + 1;
    irisu_lay_down.cell_of_loop_start  = irisu_dive.cell_of_last_frame + 1;
    irisu_lay_down.cell_of_loop_end    = irisu_dive.cell_of_last_frame + 1;
    irisu_lay_down.seconds_per_frame   = 0.08f;

    irisu_get_up_slow.tag                 = IrisuState::GET_UP_SLOW;
    irisu_get_up_slow.cell_of_first_frame = 1 * 17;
    irisu_get_up_slow.cell_of_last_frame  = irisu_get_up_slow.cell_of_first_frame + 4;
    irisu_get_up_slow.cell_of_loop_start  = irisu_get_up_slow.cell_of_first_frame;
    irisu_get_up_slow.cell_of_loop_end    = irisu_get_up_slow.cell_of_last_frame;
    irisu_get_up_slow.seconds_per_frame   = 0.08f;

    Ichigo::Sprite irisu_sprite    = {};
    irisu_sprite.width             = pixels_to_metres(60.0f);
    irisu_sprite.height            = pixels_to_metres(60.0f);
    irisu_sprite.pos_offset        = Util::calculate_centered_pos_offset(entity->col, irisu_sprite.width, irisu_sprite.height);
    irisu_sprite.sheet.texture     = irisu_texture_id;
    irisu_sprite.sheet.cell_width  = 60;
    irisu_sprite.sheet.cell_height = 60;
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

    auto process_movement_keys = [&]() {
        if (Ichigo::Internal::keyboard_state[Ichigo::IK_RIGHT].down || Ichigo::Internal::gamepad.right.down) {
            irisu->acceleration.x = irisu->movement_speed;
            SET_FLAG(irisu->flags, Ichigo::EF_FLIP_H);
        }

        if (Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT].down || Ichigo::Internal::gamepad.left.down) {
            irisu->acceleration.x = -irisu->movement_speed;
            CLEAR_FLAG(irisu->flags, Ichigo::EF_FLIP_H);
        }
    };

    bool jump_button_down_this_frame = Ichigo::Internal::keyboard_state[Ichigo::IK_SPACE].down_this_frame || Ichigo::Internal::gamepad.a.down_this_frame || Ichigo::Internal::gamepad.b.down_this_frame;
    bool jump_button_down            = Ichigo::Internal::keyboard_state[Ichigo::IK_SPACE].down || Ichigo::Internal::gamepad.a.down || Ichigo::Internal::gamepad.b.down;
    bool run_button_down             = Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT_SHIFT].down || Ichigo::Internal::gamepad.x.down || Ichigo::Internal::gamepad.y.down;
    bool dive_button_down_this_frame = Ichigo::Internal::keyboard_state[Ichigo::IK_Z].down_this_frame;

    switch (irisu_state) {
        case IDLE: {
            maybe_enter_animation(irisu, irisu_idle);
            if      (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND)) irisu_state = JUMP;
            else if (irisu->velocity.x != 0.0f)                        irisu_state = WALK;
            process_movement_keys();

            if (jump_button_down_this_frame) {
                released_jump     = false;
                applied_t         = 0.0f;
                irisu_state       = JUMP;
                irisu->velocity.y = -irisu->jump_acceleration;
                maybe_enter_animation(irisu, irisu_jump);
            }
        } break;

        case WALK: {
            maybe_enter_animation(irisu, irisu_walk);
            if      (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND))           irisu_state = JUMP;
            else if (irisu->velocity.x == 0.0f && irisu->acceleration.x == 0.0f) irisu_state = IDLE;

            process_movement_keys();

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
            maybe_enter_animation(irisu, irisu_jump);
            if      ( FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x == 0.0f) irisu_state = IDLE;
            else if ( FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x != 0.0f) irisu_state = WALK;
            else if (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.y > 0.0f)  irisu_state = FALL;
            else if (dive_button_down_this_frame) {
                irisu->velocity.y -= 5.0f;
                irisu->velocity.x += 5.0f * signof(irisu->velocity.x);
                irisu_state = DIVE;
            }

            process_movement_keys();

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
            maybe_enter_animation(irisu, irisu_fall);
            if      (FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x == 0.0f) irisu_state = IDLE;
            else if (FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x != 0.0f) irisu_state = WALK;
            else if (dive_button_down_this_frame) {
                irisu->velocity.y -= 5.0f;
                irisu->velocity.x += 5.0f * signof(irisu->velocity.x);
                irisu_state = DIVE;
                maybe_enter_animation(irisu, irisu_dive);
            }

            process_movement_keys();
        } break;

        case DIVE: {
            if (FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND)) {
                irisu_state = LAY_DOWN;
                maybe_enter_animation(irisu, irisu_lay_down);
            }

            maybe_enter_animation(irisu, irisu_dive);
        } break;

        case LAY_DOWN: {
            maybe_enter_animation(irisu, irisu_lay_down);

            if (jump_button_down_this_frame) {
                if (irisu->velocity.x == 0.0f) {
                    irisu_state = GET_UP_SLOW;
                    maybe_enter_animation(irisu, irisu_get_up_slow);
                } else {
                    irisu->velocity.y -= 5.0f;
                    irisu->velocity.x += 10.0f * signof(irisu->velocity.x);
                    irisu_state = DIVE_BOOST;
                    maybe_enter_animation(irisu, irisu_jump);
                }
            }

        } break;

        case GET_UP_SLOW: {
            maybe_enter_animation(irisu, irisu_get_up_slow);

            if (irisu->sprite.animation.tag == GET_UP_SLOW && irisu->sprite.current_animation_frame == irisu_get_up_slow.cell_of_last_frame - irisu_get_up_slow.cell_of_first_frame) {
                irisu_state = IDLE;
                maybe_enter_animation(irisu, irisu_idle);
            }

        } break;

        case DIVE_BOOST: {
            maybe_enter_animation(irisu, irisu_jump);
            if (FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND)) irisu_state = WALK;
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
