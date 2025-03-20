#include "moving_platform.hpp"
#include "ichiaji.hpp"
#include "asset_catalog.hpp"

#define MAX_PLATFORMS 64
#define MAX_ENTITIES_ON_PLATFORM 64
#define MAX_PLATFORM_AREA 64

// TODO: Maybe a fixed array isn't the right option? We are using it like a free list.
static Bana::FixedMap<Ichigo::EntityID, Bana::FixedArray<Ichigo::EntityID>> platform_entity_lists;

static void render(Ichigo::Entity *platform) {
    MAKE_STACK_ARRAY(rects, TexturedRect, MAX_PLATFORM_AREA);

    for (u32 x = 0; x < (u32) platform->col.w; ++x) {
        TexturedRect r = {
            {{(f32) x, 0.0f}, 1.0f, 1.0f},
            {{0, 0}, 32, 32} // FIXME: @magicnum
        };

        rects.append(r);
    }

    Ichigo::world_render_rect_list(platform->col.pos, rects, Assets::platform_texture_id);
}

static void update(Ichigo::Entity *platform) {
    if (Ichiaji::program_state != Ichiaji::PS_GAME) {
        return;
    }

    Vec2<f32> velocity_before = platform->velocity;
    Vec2<f32> p               = platform->col.pos;

    if (platform->user_type_id == ET_MOVING_PLATFORM) {
        platform->velocity.x = platform->movement_speed * signof(platform->velocity.x);

        Ichigo::EntityMoveResult move_result = Ichigo::move_entity_in_world(platform);

        if (move_result == Ichigo::HIT_WALL) {
            platform->velocity.x = velocity_before.x * -1.0f;
        }

        if (MIN(platform->user_data_f32_1, platform->user_data_f32_2) >= platform->col.pos.x) {
            platform->col.pos.x = MIN(platform->user_data_f32_1, platform->user_data_f32_2);
            platform->velocity.x *= -1.0f;
        }

        if (MAX(platform->user_data_f32_1, platform->user_data_f32_2) <= platform->col.pos.x) {
            platform->col.pos.x = MAX(platform->user_data_f32_1, platform->user_data_f32_2);
            platform->velocity.x *= -1.0f;
        }
    } else { // Elevator.
        platform->velocity.y = platform->movement_speed * signof(platform->velocity.y);

        Ichigo::EntityMoveResult move_result = Ichigo::move_entity_in_world(platform);

        if (move_result == Ichigo::HIT_GROUND || move_result == Ichigo::HIT_CEILING) {
            platform->velocity.y = velocity_before.y * -1.0f;
        } else {
            if (MIN(platform->user_data_f32_1, platform->user_data_f32_2) >= platform->col.pos.y) {
                platform->col.pos.y = MIN(platform->user_data_f32_1, platform->user_data_f32_2);
                platform->velocity.y *= -1.0f;
            }

            if (MAX(platform->user_data_f32_1, platform->user_data_f32_2) <= platform->col.pos.y) {
                platform->col.pos.y = MAX(platform->user_data_f32_1, platform->user_data_f32_2);
                platform->velocity.y *= -1.0f;
            }
        }
    }

    Vec2<f32> dp = platform->col.pos - p;

    auto list = platform_entity_lists.get(platform->id);
    if (!list.has_value)       return;
    if (list.value->size == 0) return;

    for (i32 i = 0; i < list.value->capacity; ++i) {
        if (!Ichigo::entity_is_dead(list.value->data[i])) {
            Ichigo::Entity *e = Ichigo::get_entity(list.value->data[i]);
            Ichigo::teleport_entity_considering_colliders(e, e->col.pos + dp);
        }
    }
}

static void on_stand(Ichigo::Entity *platform, Ichigo::Entity *other_entity, bool standing) {
    auto list = platform_entity_lists.get(platform->id);
    if (!list.has_value) return;

    if (standing) {
        if (list.value->size >= list.value->capacity) return;
        for (i32 i = 0; i < list.value->capacity; ++i) {
            if (Ichigo::entity_is_dead(list.value->data[i])) {
                list.value->data[i] = other_entity->id;
                list.value->size++;
                return;
            }
        }
    } else {
        if (list.value->size == 0) return;
        for (i32 i = 0; i < list.value->capacity; ++i) {
            if (list.value->data[i] == other_entity->id) {
                list.value->data[i] = NULL_ENTITY_ID;
                list.value->size--;
                return;
            }
        }
    }
}

static void on_kill(Ichigo::Entity *platform) {
    auto list = platform_entity_lists.get(platform->id);
    if (list.has_value) {
        std::memset(list.value->data, 0, sizeof(Ichigo::EntityID) * list.value->capacity);
        platform_entity_lists.remove(platform->id);
    }
}

void MovingPlatform::init() {
    platform_entity_lists = make_fixed_map<Ichigo::EntityID, Bana::FixedArray<Ichigo::EntityID>>(MAX_PLATFORMS, Ichigo::Internal::perm_allocator);
    for (i32 i = 0; i < platform_entity_lists.capacity; ++i) {
        platform_entity_lists.data[i].value = make_fixed_array<Ichigo::EntityID>(MAX_ENTITIES_ON_PLATFORM, Ichigo::Internal::perm_allocator);
    }
}

void MovingPlatform::spawn(const Ichigo::EntityDescriptor &descriptor) {
    assert(descriptor.custom_width * descriptor.custom_height <= (f32) MAX_PLATFORM_AREA);

    Ichigo::Entity *platform = Ichigo::spawn_entity();

    std::strcpy(platform->name, "pltfm");

    platform->col                                  = {descriptor.pos, descriptor.custom_width, descriptor.custom_height};
    platform->movement_speed                       = descriptor.movement_speed;
    platform->max_velocity                         = {9999.0f, 9999.0f};
    platform->friction_when_tangible               = 8.0f; // TODO: Custom friction?
    platform->sprite.width                         = 0.0f; // TODO: Sprite for platforms, requires a custom render proc!!
    platform->sprite.height                        = 0.0f;
    // platform->sprite.sheet.texture                 = coin_texture_id;
    platform->render_proc                          = render;
    platform->update_proc                          = update;
    platform->stand_proc                           = on_stand;
    platform->kill_proc                            = on_kill;
    platform->user_data_f32_1                      = *((f32 *) &descriptor.data);
    platform->user_data_f32_2                      = *((f32 *) ((u8 *) (&descriptor.data) + sizeof(f32)));
    platform->user_type_id                         = descriptor.type;
    platform->sprite.sheet.cell_width              = 32;
    platform->sprite.sheet.cell_height             = 32;
    platform->sprite.animation.cell_of_first_frame = 0;
    platform->sprite.animation.cell_of_last_frame  = 7;
    platform->sprite.animation.cell_of_loop_start  = 0;
    platform->sprite.animation.cell_of_loop_end    = 7;
    platform->sprite.animation.seconds_per_frame   = 0.12f;

    SET_FLAG(platform->flags, Ichigo::EF_TANGIBLE_ON_TOP);

    platform_entity_lists.slot_in(platform->id);
}

void MovingPlatform::deinit() {

}
