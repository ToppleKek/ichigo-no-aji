#include "miniboss_room.hpp"
#include "rabbit_enemy.hpp"
#include "asset_catalog.hpp"

enum CutsceneState {
    CS_NOTHING,
    CS_CAMERA_PAN,
    CS_DOOR_CLOSE,
    CS_SPAWN_MINIBOSS,
    CS_WAIT_FOR_END,
    CS_DOOR_OPEN,
};

static f32 cutscene_t                          = 0.0f;
static CutsceneState cutscene_state            = CS_NOTHING;
static Vec2<f32> camera_start_pos              = {};
static Ichigo::EntityID player_sense_entity_id = NULL_ENTITY_ID;
static Ichigo::EntityID left_door_id           = NULL_ENTITY_ID;
static Ichigo::EntityID right_door_id          = NULL_ENTITY_ID;
static Ichigo::EntityID enemies[4]             = {};
static Ichiaji::Bgm level_bgm                  = {};

#define CAMERA_PAN_T 1.25f

static void spawn_doors(Vec2<f32> controller_pos, f32 controller_width, f32 controller_height) {
    Ichigo::Entity *left_door  = Ichigo::spawn_entity();
    Ichigo::Entity *right_door = Ichigo::spawn_entity();

    left_door_id  = left_door->id;
    right_door_id = right_door->id;

    std::strcpy(left_door->name,  "boss_door");
    std::strcpy(right_door->name, "boss_door");

    left_door->col  = {{controller_pos + Vec2<f32>{-1.0f,            controller_height - 4.0f}}, 1.0f, 4.0f};
    right_door->col = {{controller_pos + Vec2<f32>{controller_width, controller_height - 4.0f}}, 1.0f, 4.0f};

    SET_FLAG(left_door->flags, Ichigo::EF_INVISIBLE);
    SET_FLAG(left_door->flags, Ichigo::EF_STATIC);
    SET_FLAG(left_door->flags, Ichigo::EF_TANGIBLE);

    SET_FLAG(right_door->flags, Ichigo::EF_INVISIBLE);
    SET_FLAG(right_door->flags, Ichigo::EF_STATIC);
    SET_FLAG(right_door->flags, Ichigo::EF_TANGIBLE);
}

static void on_collide(Ichigo::Entity *e, Ichigo::Entity *other, Vec2<f32> normal, [[maybe_unused]] Vec2<f32> collision_normal, [[maybe_unused]] Vec2<f32> collision_pos) {
    if (other->user_type_id != ET_PLAYER) return;
    if (normal.x == collision_normal.x && normal.y == collision_normal.y) return;
    if (FLAG_IS_SET(Ichiaji::current_level_save_data().progress_flags, MINIBOSS_COMPLETE_FLAG)) return;

    cutscene_state                     = CS_CAMERA_PAN;
    camera_start_pos                   = get_translation2d(Ichigo::Camera::transform) * -1.0f;
    Ichigo::Camera::manual_focus_point = camera_start_pos;
    Ichigo::Camera::mode               = Ichigo::Camera::Mode::MANUAL;
    Ichiaji::program_state             = Ichiaji::PS_CUTSCENE;
    level_bgm                          = Ichiaji::current_bgm;

    Ichiaji::change_bgm({0, 0.0f, 0.0f});

    Ichigo::kill_entity_deferred(e);
}

static void update(Ichigo::Entity *e) {
    if (FLAG_IS_SET(Ichiaji::current_level_save_data().progress_flags, MINIBOSS_COMPLETE_FLAG)) return;

    switch (cutscene_state) {
        case CS_NOTHING: {
            return;
        } break;

        case CS_CAMERA_PAN: {
            cutscene_t += Ichigo::Internal::dt;
            f32 t = clamp(cutscene_t / CAMERA_PAN_T, 0.0f, 1.0f);

            Ichigo::Camera::manual_focus_point = {
                bezier(camera_start_pos.x, camera_start_pos.x, t, e->col.pos.x, e->col.pos.x),
                bezier(camera_start_pos.y, camera_start_pos.y, t, e->col.pos.y, e->col.pos.y)
            };

            if (t == 1.0f) {
                Ichiaji::change_bgm({Assets::test_song_audio_id, 0.864f, 54.188f});
                cutscene_state = CS_DOOR_CLOSE;
            }
        } break;

        case CS_DOOR_CLOSE: {
            if (!Ichigo::entity_is_dead(left_door_id) || !Ichigo::entity_is_dead(right_door_id)) assert(false);
            spawn_doors(e->col.pos, e->col.w, e->col.h);
            cutscene_state = CS_SPAWN_MINIBOSS;
        } break;

        case CS_SPAWN_MINIBOSS: {
            Ichigo::EntityDescriptor d = {};
            for (u32 i = 0; i < ARRAY_LEN(enemies); ++i) {
                d.pos = {e->col.pos.x + e->col.w / 2.0f + (f32) i, e->col.pos.y + e->col.h - 4.0f};
                enemies[i] = RabbitEnemy::spawn(d);
            }

            cutscene_state         = CS_WAIT_FOR_END;
            Ichiaji::program_state = Ichiaji::PS_GAME;
        } break;

        case CS_WAIT_FOR_END: {
            for (u32 i = 0; i < ARRAY_LEN(enemies); ++i) {
                if (!Ichigo::entity_is_dead(enemies[i])) return;
            }

            cutscene_state = CS_DOOR_OPEN;
        } break;

        case CS_DOOR_OPEN: {
            Ichigo::kill_entity(left_door_id);
            Ichigo::kill_entity(right_door_id);

            Ichigo::Camera::follow(Ichiaji::player_entity_id);
            Ichigo::Camera::mode = Ichigo::Camera::Mode::FOLLOW;
            Ichigo::kill_entity(e);

            Ichiaji::change_bgm(level_bgm);
        } break;
    }
}

void Miniboss::spawn_controller_entity(const Ichigo::EntityDescriptor &descriptor) {
    if (FLAG_IS_SET(Ichiaji::current_level_save_data().progress_flags, MINIBOSS_COMPLETE_FLAG)) return;

    cutscene_t             = 0.0f;
    cutscene_state         = CS_NOTHING;
    camera_start_pos       = {};
    player_sense_entity_id = NULL_ENTITY_ID;
    left_door_id           = NULL_ENTITY_ID;
    right_door_id          = NULL_ENTITY_ID;
    level_bgm              = {};


    std::memset(enemies, 0, sizeof(enemies));

    Ichigo::Entity *e       = Ichigo::spawn_entity();
    Ichigo::Entity *sense_e = Ichigo::spawn_entity();

    player_sense_entity_id = sense_e->id;

    std::strcpy(e->name, "miniboss_ctrl");
    std::strcpy(sense_e->name, "miniboss_sense");

    e->col          = {descriptor.pos, descriptor.custom_width, descriptor.custom_height};
    e->update_proc  = update;

    sense_e->col          = {descriptor.pos + Vec2<f32>{2.0f, 0.0f}, descriptor.custom_width - 4.0f, descriptor.custom_height};
    sense_e->collide_proc = on_collide;

    SET_FLAG(e->flags, Ichigo::EF_STATIC);
    SET_FLAG(e->flags, Ichigo::EF_INVISIBLE);

    SET_FLAG(sense_e->flags, Ichigo::EF_STATIC);
    SET_FLAG(sense_e->flags, Ichigo::EF_INVISIBLE);
}
