#include "irisu.hpp"

EMBED("assets/irisu.png", irisu_spritesheet_png)

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

void Irisu::init(Ichigo::Entity *entity) {
    irisu_texture_id = Ichigo::load_texture(irisu_spritesheet_png, irisu_spritesheet_png_len);
    irisu_id = entity->id;

    std::strcpy(entity->name, "player");
    entity->col               = {{6.0f, 2.0f}, 0.3f, 1.1f};
    entity->max_velocity      = {8.0f, 12.0f};
    entity->movement_speed    = 20.0f;
    entity->jump_acceleration = 110.0f;
    entity->gravity           = 9.8f; // TODO: gravity should be a property of the level?
    entity->update_proc       = Irisu::update;
    entity->collide_proc      = nullptr;

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

static f32 effective_max_velocity = 4.0f;
#define IRISU_JUMP_T 0.06f
void Irisu::update(Ichigo::Entity *irisu) {
    static f32 jump_t = 0.0f;

    irisu->acceleration = {0.0f, 0.0f};
    if (Ichigo::Internal::keyboard_state[Ichigo::IK_RIGHT].down || Ichigo::Internal::gamepad.right.down) {
        irisu->acceleration.x = irisu->movement_speed;
        SET_FLAG(irisu->flags, Ichigo::EF_FLIP_H);
    }

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT].down || Ichigo::Internal::gamepad.left.down) {
        irisu->acceleration.x = -irisu->movement_speed;
        CLEAR_FLAG(irisu->flags, Ichigo::EF_FLIP_H);
    }

    bool jump_button_down = Ichigo::Internal::keyboard_state[Ichigo::IK_SPACE].down_this_frame || Ichigo::Internal::gamepad.a.down_this_frame || Ichigo::Internal::gamepad.b.down_this_frame;
    if (jump_button_down && FLAG_IS_SET(irisu->flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
        jump_t = IRISU_JUMP_T;
    }

    bool run_button_down = Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT_SHIFT].down || Ichigo::Internal::gamepad.x.down || Ichigo::Internal::gamepad.y.down;
    if (run_button_down && FLAG_IS_SET(irisu->flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
        irisu->max_velocity.x = 8.0f;
        // irisu_walk.seconds_per_frame = 0.06f;
    } else if (irisu_state != JUMP && irisu_state != FALL) {
        irisu->max_velocity.x = 4.0f;
        // irisu_walk.seconds_per_frame = 0.08f;
    }

    if (jump_t != 0.0f) {
        CLEAR_FLAG(irisu->flags, Ichigo::EntityFlag::EF_ON_GROUND);
        f32 effective_dt = jump_t < Ichigo::Internal::dt ? jump_t : Ichigo::Internal::dt;
        irisu->acceleration.y = -irisu->jump_acceleration * (effective_dt / Ichigo::Internal::dt);
        jump_t -= effective_dt;
    }

    // if (std::fabs(irisu->velocity.x) >= effective_max_velocity) {
    //     irisu->acceleration.x = 0.0f;
    // }

    Ichigo::move_entity_in_world(irisu);

    switch (irisu_state) {
        case IDLE: {
            if      (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND)) irisu_state = JUMP;
            else if (irisu->velocity.x != 0.0f)                        irisu_state = WALK;
            else if (irisu->sprite.animation.tag != IDLE) {
                irisu->sprite.animation                    = irisu_idle;
                irisu->sprite.current_animation_frame      = 0;
                irisu->sprite.elapsed_animation_frame_time = 0.0f;
            }
        } break;

        case WALK: {
            if      (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND))           irisu_state = JUMP;
            else if (irisu->velocity.x == 0.0f && irisu->acceleration.x == 0.0f) irisu_state = IDLE;
            else if (irisu->sprite.animation.tag != WALK) {
                irisu->sprite.animation                    = irisu_walk;
                irisu->sprite.current_animation_frame      = 0;
                irisu->sprite.elapsed_animation_frame_time = 0.0f;
            }
        } break;

        case JUMP: {
            if      ( FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x == 0.0f && irisu->acceleration.x == 0.0f) irisu_state = IDLE;
            else if ( FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x != 0.0f) irisu_state = WALK;
            else if (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.y > 0.0f)  irisu_state = FALL;
            else if (irisu->sprite.animation.tag != JUMP) {
                irisu->sprite.animation                    = irisu_jump;
                irisu->sprite.current_animation_frame      = 0;
                irisu->sprite.elapsed_animation_frame_time = 0.0f;
            }
        } break;

        case FALL: {
            if      (FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x == 0.0f && irisu->acceleration.x == 0.0f) irisu_state = IDLE;
            else if (FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x != 0.0f) irisu_state = WALK;
            else if (irisu->sprite.animation.tag != FALL) {
                irisu->sprite.animation                    = irisu_fall;
                irisu->sprite.current_animation_frame      = 0;
                irisu->sprite.elapsed_animation_frame_time = 0.0f;
            }
        } break;
    }
}
