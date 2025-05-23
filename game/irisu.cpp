/*
    Ichigo demo game code.

    Game logic for player character.

    Author: Braeden Hong
    Date:   2025/03/12
*/

#include "irisu.hpp"
#include "ichiaji.hpp"
#include "particle_source.hpp"
#include "entrances.hpp"
#include "ui.hpp"
#include "asset_catalog.hpp"
#include "../util.hpp"

enum IrisuState {
    IDLE,
    WALK,
    JUMP,
    FALL,
    DIVE,
    HURT,
    LAY_DOWN,
    GET_UP_SLOW,
    DIVE_BOOST,
    RESPAWNING,
    DEAD
};

enum SpellType {
    ST_NORMAL,
    ST_FIRE
};

#define IRISU_DEFAULT_GRAVITY 14.0f
#define IRISU_FALL_GRAVITY (IRISU_DEFAULT_GRAVITY * 1.75f)
#define IRISU_MAX_JUMP_T 0.20f
#define DIVE_X_VELOCITY 2.0f
#define DIVE_Y_VELOCITY 4.0f
#define IRISU_DEFAULT_JUMP_ACCELERATION 6.0f
#define RESPAWN_COOLDOWN_T 0.75f
#define RESPAWN_DAMAGE 2.0f
#define FIRE_SPELL_ANIMATION_FRAMES 4

static SpellType current_spell_type        = ST_NORMAL;
static IrisuState irisu_state              = IrisuState::IDLE;
static Ichigo::Animation irisu_idle        = {};
static Ichigo::Animation irisu_walk        = {};
static Ichigo::Animation irisu_jump        = {};
static Ichigo::Animation irisu_fall        = {};
static Ichigo::Animation irisu_dive        = {};
static Ichigo::Animation irisu_hurt        = {};
static Ichigo::Animation irisu_lay_down    = {};
static Ichigo::Animation irisu_get_up_slow = {};
static Ichigo::EntityID irisu_id           = {};

static Ichigo::Sprite spell_sprite;
static Ichigo::Sprite fire_spell_sprite;
static Ichigo::Sprite irisu_sprite;

static f32 irisu_collider_width  = 0.3f;
static f32 irisu_collider_height = 1.1f;

// TODO: Change this such that all types of entrances store the entity ID and call Entrance::can_enter_entrance(EntityID) function or something.
// static i64 entrance_to_enter                     = -1;
// static i64 level_to_enter                        = -1;
// static bool in_front_of_shop                     = false;
static Ichigo::EntityID interaction_entity = NULL_ENTITY_ID; // The entity that we are standing in front of- ie. the one we will try to interact with if the player presses up.
static Vec2<f32> last_entrance_position          = {};

static f32 invincibility_t = 0.0f;
static f32 respawn_t       = 0.0f;

static void snap_to_floor_or_fall(Ichigo::Entity *irisu) {
   auto tg = Ichigo::nearest_tangible_ground_tile(irisu);
    if (tg.has_value) {
        auto ground = tg.value;
        if (DISTANCE(ground.y, irisu->col.pos.y + irisu->col.h) < 1.0f) {
            // FIXME: Hack...
            auto saved_v      = irisu->velocity;
            irisu->velocity.y = 9999.0f;
            irisu->velocity.x = 0.0f;
            Ichigo::move_entity_in_world(irisu);
            irisu->velocity   = saved_v;
        } else {
            CLEAR_FLAG(irisu->flags, Ichigo::EF_ON_GROUND);
        }
    } else {
        CLEAR_FLAG(irisu->flags, Ichigo::EF_ON_GROUND);
    }
}

static void try_enter_entrance(Vec2<f32> exit_location) {
    auto callback = [](uptr data) {
        Vec2<f32> exit_position = *((Vec2<f32> *) &data);
        auto enable_input = []([[maybe_unused]] uptr data) {
            Ichiaji::input_disabled = false;
        };

        Ichiaji::fullscreen_transition({0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, 0.3f, enable_input, 0);
        Ichigo::Camera::mode   = Ichigo::Camera::Mode::FOLLOW;
        auto *irisu            = Ichigo::get_entity(irisu_id);
        last_entrance_position = exit_position;
        Ichigo::teleport_entity_considering_colliders(irisu, exit_position);

        // Snap to the ground if we are close enough. If not, start falling.
        snap_to_floor_or_fall(irisu);
    };

    Ichigo::Camera::mode               = Ichigo::Camera::Mode::MANUAL;
    Ichigo::Camera::manual_focus_point = get_translation2d(Ichigo::Camera::transform) * -1.0f;
    Ichiaji::input_disabled            = true;
    Ichiaji::fullscreen_transition({0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 0.3f, callback, *((uptr *) &exit_location));
}

static void try_enter_new_level(i64 level_index) {
    auto callback = [](uptr data) {
        auto enable_input = []([[maybe_unused]] uptr data) {
            Ichiaji::input_disabled = false;
        };

        Ichiaji::fullscreen_transition({0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, 0.3f, enable_input, 0);
        Ichiaji::try_change_level((u32) data);
        Ichiaji::current_save_data.player_data.level_id = data;
    };

    Ichiaji::input_disabled = true;
    Ichiaji::fullscreen_transition({0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 0.3f, callback, (uptr) level_index);
}

#define GERT_DAMAGE 1.0f
#define RABBIT_DAMAGE 2.0f
static void on_collide(Ichigo::Entity *irisu, Ichigo::Entity *other, Vec2<f32> normal, [[maybe_unused]] Vec2<f32> collision_normal, [[maybe_unused]] Vec2<f32> collision_pos) {
    if ((normal.x != collision_normal.x || normal.y != collision_normal.y) && other->id == interaction_entity) {
        interaction_entity = NULL_ENTITY_ID;
    } else {
        interaction_entity = other->id;
    }

    switch (other->user_type_id) {
        case ET_RABBIT:
        case ET_GERT: {
            if (irisu_state != HURT && invincibility_t == 0.0f) {
                irisu->acceleration = {0.0f, 0.0f};
                irisu->velocity.y   = -3.0f;
                irisu->velocity.x   = 3.0f * (FLAG_IS_SET(irisu->flags, Ichigo::EF_FLIP_H) ? -1 : 1);
                irisu_state         = HURT;

                Ichiaji::current_save_data.player_data.health -= other->user_type_id == ET_GERT ? GERT_DAMAGE : RABBIT_DAMAGE;
            }
        } break;

        case ET_ENTRANCE_TRIGGER:
        case ET_ENTRANCE_TRIGGER_H: {
            // NOTE: Don't try to enter another entrance if we are already entering one (input is disabled) or we are in a cutscene.
            if (!Ichiaji::input_disabled) {
                try_enter_entrance(Vec2<f32>{other->user_data_f32_1, other->user_data_f32_2});
            }
        } break;

        case ET_COIN: {
            Ichigo::Mixer::play_audio_oneshot(Assets::coin_collect_audio_id, 1.0f, 1.0f, 1.0f);
            Ichiaji::current_save_data.player_data.money += COIN_VALUE;
            Ichigo::kill_entity_deferred(other);
        } break;

        case ET_RECOVERY_HEART: {
            Ichigo::Mixer::play_audio_oneshot(Assets::heart_collect_audio_id, 1.0f, 1.0f, 1.0f);
            Ichiaji::current_save_data.player_data.health = clamp(Ichiaji::current_save_data.player_data.health + RECOVERY_HEART_VALUE, 0.0f, PLAYER_STARTING_HEALTH + Ichiaji::player_bonuses.max_health);
            Ichigo::kill_entity_deferred(other);
        } break;

        case ET_HP_UP_COLLECTABLE: {
            Ichigo::Mixer::play_audio_oneshot(Assets::heart_collect_audio_id, 1.0f, 1.0f, 1.0f);
            Ichiaji::give_item((Ichiaji::InventoryItem) other->user_data_i64);
            Ichigo::show_info("Max HP increased!");
            Ichigo::kill_entity_deferred(other);
        } break;

        case ET_ATTACK_SPEED_UP_COLLECTABLE: {
            Ichigo::Mixer::play_audio_oneshot(Assets::as_up_collect_audio_id, 1.0f, 1.0f, 1.0f);
            Ichiaji::give_item((Ichiaji::InventoryItem) other->user_data_i64);
            Ichigo::show_info("Attack speed increased!");
            Ichigo::kill_entity_deferred(other);
        } break;

        case ET_FIRE_SPELL_COLLECTABLE: {
            Ichigo::Mixer::play_audio_oneshot(Assets::as_up_collect_audio_id, 1.0f, 1.0f, 1.0f);
            Ichiaji::give_item(Ichiaji::INV_FIRE_SPELL);
            Ichigo::kill_entity_deferred(other);
            Ui::open_dialogue_ui(DIALOGUE_FIRE_SPELL_EXPLAIN, ARRAY_LEN(DIALOGUE_FIRE_SPELL_EXPLAIN));
        } break;

        default: break;
    }
}

void Irisu::init() {
    irisu_idle.tag                 = IrisuState::IDLE;
    irisu_idle.cell_of_first_frame = 6 * 17;
    irisu_idle.cell_of_last_frame  = irisu_idle.cell_of_first_frame + 7;
    irisu_idle.cell_of_loop_start  = irisu_idle.cell_of_first_frame;
    irisu_idle.cell_of_loop_end    = irisu_idle.cell_of_last_frame;
    irisu_idle.seconds_per_frame   = 0.08f;

    irisu_walk.tag                 = IrisuState::WALK;
    irisu_walk.cell_of_first_frame = 5 * 17;
    irisu_walk.cell_of_last_frame  = irisu_walk.cell_of_first_frame + 4;
    irisu_walk.cell_of_loop_start  = irisu_walk.cell_of_first_frame;
    irisu_walk.cell_of_loop_end    = irisu_walk.cell_of_last_frame;
    irisu_walk.seconds_per_frame   = 0.1f;

    irisu_jump.tag                 = IrisuState::JUMP;
    irisu_jump.cell_of_first_frame = 4 * 17;
    irisu_jump.cell_of_last_frame  = irisu_jump.cell_of_first_frame + 3;
    irisu_jump.cell_of_loop_start  = irisu_jump.cell_of_first_frame + 1;
    irisu_jump.cell_of_loop_end    = irisu_jump.cell_of_last_frame;
    irisu_jump.seconds_per_frame   = 0.08f;

    irisu_fall.tag                 = IrisuState::FALL;
    irisu_fall.cell_of_first_frame = 3 * 17;
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

    irisu_hurt.tag                 = IrisuState::HURT;
    irisu_hurt.cell_of_first_frame = 1 * 17;
    irisu_hurt.cell_of_last_frame  = irisu_hurt.cell_of_first_frame + 1;
    irisu_hurt.cell_of_loop_start  = irisu_hurt.cell_of_first_frame;
    irisu_hurt.cell_of_loop_end    = irisu_hurt.cell_of_last_frame;
    irisu_hurt.seconds_per_frame   = 0.08f;

    irisu_lay_down.tag                 = IrisuState::LAY_DOWN;
    // The "lay down" cell is one frame after the end of the dive animation.
    irisu_lay_down.cell_of_first_frame = irisu_dive.cell_of_last_frame + 1;
    irisu_lay_down.cell_of_last_frame  = irisu_dive.cell_of_last_frame + 1;
    irisu_lay_down.cell_of_loop_start  = irisu_dive.cell_of_last_frame + 1;
    irisu_lay_down.cell_of_loop_end    = irisu_dive.cell_of_last_frame + 1;
    irisu_lay_down.seconds_per_frame   = 0.08f;

    irisu_get_up_slow.tag                 = IrisuState::GET_UP_SLOW;
    irisu_get_up_slow.cell_of_first_frame = 2 * 17;
    irisu_get_up_slow.cell_of_last_frame  = irisu_get_up_slow.cell_of_first_frame + 4;
    irisu_get_up_slow.cell_of_loop_start  = irisu_get_up_slow.cell_of_first_frame;
    irisu_get_up_slow.cell_of_loop_end    = irisu_get_up_slow.cell_of_last_frame;
    irisu_get_up_slow.seconds_per_frame   = 0.12f;

    irisu_sprite.width             = pixels_to_metres(60.0f);
    irisu_sprite.height            = pixels_to_metres(60.0f);
    irisu_sprite.pos_offset        = Util::calculate_centered_pos_offset(irisu_collider_width, irisu_collider_height, irisu_sprite.width, irisu_sprite.height);
    irisu_sprite.sheet.texture     = Assets::irisu_texture_id;
    irisu_sprite.sheet.cell_width  = 60;
    irisu_sprite.sheet.cell_height = 60;
    irisu_sprite.animation         = irisu_idle;

    const Ichigo::Texture &spell_texture = Ichigo::Internal::textures[Assets::spell_texture_id];
    spell_sprite.width             = pixels_to_metres(spell_texture.width);
    spell_sprite.height            = pixels_to_metres(spell_texture.height);
    spell_sprite.pos_offset        = {0.0f, 0.0f};
    spell_sprite.sheet.texture     = Assets::spell_texture_id;
    spell_sprite.sheet.cell_width  = spell_texture.width;
    spell_sprite.sheet.cell_height = spell_texture.height;
    spell_sprite.animation         = {0, 0, 0, 0, 0, 0.0f};

    const Ichigo::Texture &fire_spell_texture = Ichigo::Internal::textures[Assets::fire_spell_texture_id];
    fire_spell_sprite.width             = pixels_to_metres(fire_spell_texture.width) / (f32) FIRE_SPELL_ANIMATION_FRAMES;
    fire_spell_sprite.height            = pixels_to_metres(fire_spell_texture.height);
    fire_spell_sprite.pos_offset        = {0.0f, 0.0f};
    fire_spell_sprite.sheet.texture     = Assets::fire_spell_texture_id;
    fire_spell_sprite.sheet.cell_width  = fire_spell_texture.width / FIRE_SPELL_ANIMATION_FRAMES;
    fire_spell_sprite.sheet.cell_height = fire_spell_texture.height;
    fire_spell_sprite.animation         = {0, 0, 3, 0, 3, 0.12f};
}

void Irisu::spawn(Ichigo::Entity *entity, Vec2<f32> pos) {
    Ichigo::change_entity_draw_layer(entity, 16);

    invincibility_t        = 0.0f;
    respawn_t              = 0.0f;
    irisu_state            = IDLE;
    interaction_entity     = NULL_ENTITY_ID;
    last_entrance_position = pos; // FIXME: When you save the game, you will end up wherever you saved if you are respawned. Should we save the last entrance position as well?
    irisu_id               = entity->id;

    std::strcpy(entity->name, "player");
    entity->col               = {pos, 0.3f, 1.1f};
    entity->max_velocity      = {8.0f, 12.0f};
    entity->movement_speed    = 16.0f;
    entity->jump_acceleration = IRISU_DEFAULT_JUMP_ACCELERATION;
    entity->gravity           = IRISU_DEFAULT_GRAVITY; // TODO: gravity should be a property of the level?
    entity->update_proc       = Irisu::update;
    entity->collide_proc      = on_collide;
    entity->user_type_id      = ET_PLAYER;

    entity->sprite = irisu_sprite;
}

void Irisu::deinit() {

}

static inline void maybe_enter_animation(Ichigo::Entity *entity, Ichigo::Animation animation) {
    if (entity->sprite.animation.tag != animation.tag) {
        entity->sprite.animation                    = animation;
        entity->sprite.current_animation_frame      = 0;
        entity->sprite.elapsed_animation_frame_time = 0.0f;
    }
}

struct InputState {
    bool jump_button_down_this_frame;
    bool jump_button_down;
    bool run_button_down;
    bool dive_button_down_this_frame;
    bool up_button_down_this_frame;
};

static void process_movement_keys(Ichigo::Entity *irisu) {
    if (Ichiaji::input_disabled) return;

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_RIGHT].down || Ichigo::Internal::gamepad.right.down) {
        irisu->acceleration.x = irisu->movement_speed;
        SET_FLAG(irisu->flags, Ichigo::EF_FLIP_H);
    }

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT].down || Ichigo::Internal::gamepad.left.down) {
        irisu->acceleration.x = -irisu->movement_speed;
        CLEAR_FLAG(irisu->flags, Ichigo::EF_FLIP_H);
    }
}

static void make_sparks(Vec2<f32> pos) {
    Ichigo::EntityDescriptor spark_particle_gen_descriptor = {
        "DYN_SPARK_PARTICLES",
        9999, // NOTE: Unused ID
        pos,
        1.0f,
        1.0f,
        0.0f,
        0
    };

    ParticleSource::spawn(spark_particle_gen_descriptor, Assets::sparks_texture_id, 0.1f, 0.3f, 3.0f);
}

#define SPELL_MAX_VELOCITY 24.0f
#define DEFAULT_SPELL_COOLDOWN 0.5f
static void spell_update(Ichigo::Entity *spell) {
    if (Ichiaji::program_state != Ichiaji::PS_GAME) {
        return;
    }

    spell->velocity = {spell->velocity.x < 0.0f ? -SPELL_MAX_VELOCITY : SPELL_MAX_VELOCITY, 0.0f};

    Ichigo::EntityMoveResult move_result = Ichigo::move_entity_in_world(spell);
    if (move_result == Ichigo::HIT_WALL) {
        make_sparks(spell->col.pos);
        Ichigo::kill_entity_deferred(spell);
    }
}

static void spell_collide(Ichigo::Entity *spell, [[maybe_unused]] Ichigo::Entity *other, [[maybe_unused]] Vec2<f32> normal, [[maybe_unused]] Vec2<f32> collision_normal, [[maybe_unused]] Vec2<f32> collision_pos) {
    if (FLAG_IS_SET(other->flags, Ichigo::EF_TANGIBLE)) {
        make_sparks(spell->col.pos);
        Ichigo::kill_entity_deferred(spell);
    }

    switch (other->user_type_id) {
        case ET_RABBIT:
        case ET_GERT: {
            make_sparks(spell->col.pos);
            Ichigo::kill_entity_deferred(spell);
        } break;

        default: break;
    }
}

static f32 attack_cooldown_remaining = 0.0f;
static void cast_spell(Ichigo::Entity *irisu) {
    Ichigo::Entity *spell = Ichigo::spawn_entity();
    spell->max_velocity   = {SPELL_MAX_VELOCITY, 0.0f};
    spell->movement_speed = 100.0f;
    spell->gravity        = 0.0f;
    spell->update_proc    = spell_update;
    spell->collide_proc   = spell_collide;

    if (FLAG_IS_SET(irisu->flags, Ichigo::EF_FLIP_H)) {
        spell->velocity.x = SPELL_MAX_VELOCITY;
        spell->col.pos += Vec2<f32>{irisu->col.w, 0.0f};
    } else {
        SET_FLAG(spell->flags, Ichigo::EF_FLIP_H);
        spell->velocity.x = -SPELL_MAX_VELOCITY;
        spell->col.pos += Vec2<f32>{-spell_sprite.width, 0.0f};
    }

    if (current_spell_type == ST_NORMAL) {
        std::strcpy(spell->name, "spell");
        spell->col            = {irisu->col.pos + Vec2<f32>{0.0f, 0.2f}, spell_sprite.width, spell_sprite.height};
        spell->sprite         = spell_sprite;
        spell->user_type_id   = ET_SPELL;

        Ichigo::Mixer::play_audio_oneshot(Assets::spell_audio_id, 1.0f, 1.0f, 1.0f);
    } else if (current_spell_type == ST_FIRE) {
        std::strcpy(spell->name, "fire_spell");
        spell->col            = {irisu->col.pos + Vec2<f32>{(FLAG_IS_SET(irisu->flags, Ichigo::EF_FLIP_H) ? irisu->col.w : -fire_spell_sprite.width), 0.2f}, fire_spell_sprite.width, fire_spell_sprite.height};
        spell->sprite         = fire_spell_sprite;
        spell->user_type_id   = ET_FIRE_SPELL;
        Ichigo::Mixer::play_audio_oneshot(Assets::fire_audio_id, 1.0f, 1.0f, 1.0f);
    }

    attack_cooldown_remaining = DEFAULT_SPELL_COOLDOWN - Ichiaji::player_bonuses.attack_speed; // NOTE: This attack speed bonus could be a % of the default instead?
}

void Irisu::update(Ichigo::Entity *irisu) {
    if (Ichiaji::program_state != Ichiaji::PS_GAME) {
        return;
    }

    static f32 applied_t = 0.0f;
    static bool released_jump = false;

    if (attack_cooldown_remaining > 0.0f) {
        attack_cooldown_remaining -= Ichigo::Internal::dt;
    }

    irisu->acceleration = {0.0f, 0.0f};

    static f32 invincible_flash_t = 0.0f;

    if (invincibility_t > 0.0f) {
        invincibility_t -= Ichigo::Internal::dt;
        invincible_flash_t += Ichigo::Internal::dt;

        if (invincible_flash_t > 0.05f) {
            invincible_flash_t = 0.0f;
            if (FLAG_IS_SET(irisu->flags, Ichigo::EF_INVISIBLE)) CLEAR_FLAG(irisu->flags, Ichigo::EF_INVISIBLE);
            else                                                 SET_FLAG  (irisu->flags, Ichigo::EF_INVISIBLE);
        }

        if (invincibility_t < 0.0f) {
            invincibility_t = 0.0f;
            CLEAR_FLAG(irisu->flags, Ichigo::EF_INVISIBLE);
        }
    }

    bool jump_button_down_this_frame         = !Ichiaji::input_disabled && (Ichigo::Internal::keyboard_state[Ichigo::IK_SPACE].down_this_frame || Ichigo::Internal::gamepad.a.down_this_frame || Ichigo::Internal::gamepad.b.down_this_frame);
    bool jump_button_down                    = !Ichiaji::input_disabled && (Ichigo::Internal::keyboard_state[Ichigo::IK_SPACE].down || Ichigo::Internal::gamepad.a.down || Ichigo::Internal::gamepad.b.down);
    bool run_button_down                     = !Ichiaji::input_disabled && (Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT_SHIFT].down || Ichigo::Internal::gamepad.x.down || Ichigo::Internal::gamepad.y.down);
    bool dive_button_down_this_frame         = !Ichiaji::input_disabled && (Ichigo::Internal::keyboard_state[Ichigo::IK_Z].down_this_frame || Ichigo::Internal::gamepad.lb.down_this_frame || Ichigo::Internal::gamepad.rb.down_this_frame);
    bool up_button_down_this_frame           = !Ichiaji::input_disabled && (Ichigo::Internal::keyboard_state[Ichigo::IK_UP].down_this_frame || Ichigo::Internal::gamepad.up.down_this_frame);
    bool fire_button_down                    = !Ichiaji::input_disabled && (Ichigo::Internal::keyboard_state[Ichigo::IK_X].down || Ichigo::Internal::gamepad.stick_left_click.down); // TODO: wtf lol
    bool spell_switch_button_down_this_frame = !Ichiaji::input_disabled && (Ichigo::Internal::keyboard_state[Ichigo::IK_TAB].down_this_frame || Ichigo::Internal::gamepad.select.down_this_frame);

    if (spell_switch_button_down_this_frame) {
        if (current_spell_type == ST_NORMAL && Ichiaji::item_obtained(Ichiaji::INV_FIRE_SPELL)) {
            Ichigo::show_info("Fire spell active.");
            current_spell_type = ST_FIRE;
        } else {
            Ichigo::show_info("Normal spell active.");
            current_spell_type = ST_NORMAL;
        }
    }

    irisu->gravity = IRISU_DEFAULT_GRAVITY;

    if (irisu_state != DEAD && Ichiaji::current_save_data.player_data.health <= 0.0f) {
        irisu_state = DEAD;
        Ui::open_game_over_ui();
    }

    switch (irisu_state) {
        case IDLE: {
            maybe_enter_animation(irisu, irisu_idle);

            if (fire_button_down && attack_cooldown_remaining <= 0.0f) {
                cast_spell(irisu);
            }

            if      (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND)) irisu_state = JUMP;
            else if (irisu->velocity.x != 0.0f)                        irisu_state = WALK;
            process_movement_keys(irisu);

            if (up_button_down_this_frame) {
                auto *e = Ichigo::get_entity(interaction_entity);

                if (e) {
                    switch (e->user_type_id) {
                        case ET_ENTRANCE: {
                            try_enter_entrance(Vec2<f32>{e->user_data_f32_1, e->user_data_f32_2});
                            // Reset the interaction entity since we are leaving this area.
                            interaction_entity = NULL_ENTITY_ID;
                        } break;

                        case ET_LOCKED_DOOR: {
                            auto exit_location = Entrances::get_exit_location_if_possible(interaction_entity);
                            if (exit_location.has_value) {
                                try_enter_entrance(exit_location.value);
                                interaction_entity = NULL_ENTITY_ID;
                            }
                        } break;

                        case ET_LEVEL_ENTRANCE: {
                            try_enter_new_level(e->user_data_i64);
                            interaction_entity = NULL_ENTITY_ID;
                        } break;

                        case ET_SHOP_ENTRANCE: {
                            Ui::open_shop_ui();
                        } break;

                        case ET_BUCHOU: {
                            Ichiaji::try_talk_to(e);
                        } break;

                        case ET_SAVE_STATUE: {
                            if (Ichiaji::save_game()) {
                                Ichigo::show_info("Saved!");
                            } else {
                                Ichigo::show_info("Failed to save game.");
                            }
                        } break;
                    }
                }
            }

            if (jump_button_down_this_frame) {
                released_jump     = false;
                applied_t         = 0.0f;
                irisu_state       = JUMP;
                irisu->velocity.y = -irisu->jump_acceleration;
                maybe_enter_animation(irisu, irisu_jump);
                Ichigo::Mixer::play_audio_oneshot(Assets::jump_audio_id, 1.0f, 1.0f, 1.0f);
            }
        } break;

        case WALK: {
            maybe_enter_animation(irisu, irisu_walk);

            if (fire_button_down && attack_cooldown_remaining <= 0.0f) {
                cast_spell(irisu);
            }

            if (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND)) {
                irisu_state = JUMP;
            } else {
                process_movement_keys(irisu);

                if (irisu->velocity.x == 0.0f && irisu->acceleration.x == 0.0f) {
                    irisu_state = IDLE;
                }

                if (jump_button_down_this_frame) {
                    released_jump     = false;
                    applied_t         = 0.0f;
                    irisu_state       = JUMP;
                    irisu->velocity.y = -irisu->jump_acceleration;
                    maybe_enter_animation(irisu, irisu_jump);
                    Ichigo::Mixer::play_audio_oneshot(Assets::jump_audio_id, 1.0f, 1.0f, 1.0f);
                }

                if (run_button_down) {
                    irisu->max_velocity.x = 8.0f;
                    // irisu_walk.seconds_per_frame = 0.06f;
                } else {
                    irisu->max_velocity.x = 4.0f;
                    // irisu_walk.seconds_per_frame = 0.08f;
                }
            }
        } break;

        case JUMP: {
            maybe_enter_animation(irisu, irisu_jump);

            if      ( FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x == 0.0f && irisu->acceleration.x == 0.0f) irisu_state = IDLE;
            else if ( FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x != 0.0f) irisu_state = WALK;
            else if (!FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.y > 0.0f)  irisu_state = FALL;
            else if (dive_button_down_this_frame) {
                irisu->velocity.y -= DIVE_Y_VELOCITY;
                irisu->velocity.x += DIVE_X_VELOCITY * (FLAG_IS_SET(irisu->flags, Ichigo::EF_FLIP_H) ? 1 : -1);
                irisu_state = DIVE;
            } else {
                process_movement_keys(irisu);

                if (!jump_button_down && !released_jump) {
                    released_jump = true;
                }

                if (fire_button_down && attack_cooldown_remaining <= 0.0f) {
                    cast_spell(irisu);
                }

                if (jump_button_down && !released_jump && applied_t < IRISU_MAX_JUMP_T) {
                    CLEAR_FLAG(irisu->flags, Ichigo::EntityFlag::EF_ON_GROUND);
                    f32 effective_dt = MIN(Ichigo::Internal::dt, IRISU_MAX_JUMP_T - applied_t);
                    irisu->velocity.y = -irisu->jump_acceleration;
                    applied_t += effective_dt;
                }
            }
        } break;

        case FALL: {
            maybe_enter_animation(irisu, irisu_fall);
            irisu->gravity = IRISU_FALL_GRAVITY;

            if      (FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x == 0.0f && irisu->acceleration.x == 0.0f) irisu_state = IDLE;
            else if (FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && irisu->velocity.x != 0.0f) irisu_state = WALK;
            else if (dive_button_down_this_frame) {
                irisu->velocity.y -= DIVE_Y_VELOCITY;
                irisu->velocity.x += DIVE_X_VELOCITY * (FLAG_IS_SET(irisu->flags, Ichigo::EF_FLIP_H) ? 1 : -1);
                irisu_state = DIVE;
                maybe_enter_animation(irisu, irisu_dive);
            } else {
                process_movement_keys(irisu);

                if (fire_button_down && attack_cooldown_remaining <= 0.0f) {
                    cast_spell(irisu);
                }

            }
        } break;

        case DIVE: {
            maybe_enter_animation(irisu, irisu_dive);
            irisu->gravity = IRISU_FALL_GRAVITY;

            if (FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND)) {
                irisu_state = LAY_DOWN;
                maybe_enter_animation(irisu, irisu_lay_down);
            }
        } break;

        case HURT: {
            static f32 hurt_t = 0.0f;
            maybe_enter_animation(irisu, irisu_hurt);

            if (hurt_t == 0.0f) {
                hurt_t = 0.6f;
            } else {
                hurt_t -= Ichigo::Internal::dt;
                if (hurt_t <= 0.0f) {
                    hurt_t          = 0.0f;
                    invincibility_t = 1.5f;
                    irisu_state     = IDLE;
                    maybe_enter_animation(irisu, irisu_idle);
                }
            }
        } break;

        case LAY_DOWN: {
            maybe_enter_animation(irisu, irisu_lay_down);

            if (jump_button_down_this_frame) {
                if (irisu->velocity.x == 0.0f) {
                    irisu_state = GET_UP_SLOW;
                    maybe_enter_animation(irisu, irisu_get_up_slow);
                } else {
                    irisu->velocity.y -= 5.0f;
                    irisu->velocity.x += DIVE_X_VELOCITY * 2.5f * signof(irisu->velocity.x);
                    irisu_state = DIVE_BOOST;
                    maybe_enter_animation(irisu, irisu_jump);
                    Ichigo::Mixer::play_audio_oneshot(Assets::jump_audio_id, 1.0f, 1.0f, 1.0f);
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

        case RESPAWNING: {
            maybe_enter_animation(irisu, irisu_hurt);
            respawn_t -= Ichigo::Internal::dt;

            if (respawn_t <= 0.0f) {
                auto callback = [](uptr data) {
                    Ichiaji::fullscreen_transition({0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, 0.3f, nullptr, 0);
                    auto irisu = Ichigo::get_entity(BIT_CAST(Ichigo::EntityID, data));

                    if (irisu) {
                        Ichigo::teleport_entity_considering_colliders(irisu, last_entrance_position);
                        snap_to_floor_or_fall(irisu);
                    }

                    irisu_state = IDLE;
                    maybe_enter_animation(irisu, irisu_idle);
                };

                Ichiaji::fullscreen_transition({0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 0.3f, callback, BIT_CAST(uptr, irisu->id));
                respawn_t = 999.0f; // FIXME: @hack lol. Just so the transition can complete.
            }
        } break;

        case DEAD: {
            maybe_enter_animation(irisu, irisu_hurt);
        } break;
    }

    Ichigo::EntityMoveResult move_result = Ichigo::move_entity_in_world(irisu);

    if (irisu->velocity.y == 0.0f) {
        released_jump = true;
    }

    // Prevent the player from walking in place when pushing up against a wall.
    if (move_result == Ichigo::HIT_WALL && irisu_state == WALK) {
        irisu_state = IDLE;
        maybe_enter_animation(irisu, irisu_idle);
    }

    Ichigo::TileID left_tile_id  = Ichigo::tile_at(irisu->left_standing_tile);
    Ichigo::TileID right_tile_id = Ichigo::tile_at(irisu->right_standing_tile);

    if (left_tile_id != ICHIGO_INVALID_TILE && right_tile_id != ICHIGO_INVALID_TILE) {
        Ichigo::TileInfo left_tile_info  = Ichigo::get_tile_info(left_tile_id);
        Ichigo::TileInfo right_tile_info = Ichigo::get_tile_info(right_tile_id);

        if (irisu_state != RESPAWNING && FLAG_IS_SET(irisu->flags, Ichigo::EF_ON_GROUND) && (FLAG_IS_SET(left_tile_info.flags, Ichigo::TF_LETHAL) || FLAG_IS_SET(right_tile_info.flags, Ichigo::TF_LETHAL))) {
            respawn_t           = RESPAWN_COOLDOWN_T;
            irisu_state         = RESPAWNING;
            irisu->velocity     = {0.0f, 0.0f};
            irisu->acceleration = {0.0f, 0.0f};

            Ichiaji::current_save_data.player_data.health -= RESPAWN_DAMAGE;

            maybe_enter_animation(irisu, irisu_hurt);
        }
    }

    Ichiaji::current_save_data.player_data.position = irisu->col.pos;
}
