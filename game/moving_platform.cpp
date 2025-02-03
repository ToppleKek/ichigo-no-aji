#include "moving_platform.hpp"

#define MAX_PLATFORMS 64
#define MAX_ENTITIES_ON_PLATFORM 64
#define MAX_PLATFORM_AREA 64

EMBED("assets/moving-platform.png", platform_spritesheet_png)

// TODO: Maybe a fixed array isn't the right option? We are using it like a free list.
static Bana::FixedMap<Ichigo::EntityID, Bana::FixedArray<Ichigo::EntityID>> platform_entity_lists;
static Ichigo::TextureID platform_spritesheet_texture;

static void render(Ichigo::Entity *platform) {
    MAKE_STACK_ARRAY(rects, TexturedRect, MAX_PLATFORM_AREA);

    for (u32 x = 0; x < (u32) platform->col.w; ++x) {
        TexturedRect r = {
            {{(f32) x, 0.0f}, 1.0f, 1.0f},
            {{0, 0}, 32, 32}

        };

        rects.append(r);
    }

    ICHIGO_INFO("RECT COUNT: %d", rects.size);

    Ichigo::world_render_rect_list(platform->col.pos, rects, platform_spritesheet_texture);
}

static void update(Ichigo::Entity *platform) {
    platform->velocity.x = platform->movement_speed * signof(platform->velocity.x);
    f32 velocity_before  = platform->velocity.x;

    [[maybe_unused]] Ichigo::EntityMoveResult move_result = Ichigo::move_entity_in_world(platform);

    if (move_result == Ichigo::HIT_WALL) {
        platform->velocity.x = velocity_before * -1.0f;
    }

    if (MIN(platform->user_data_f32_1, platform->user_data_f32_2) >= platform->col.pos.x) {
        platform->col.pos.x = MIN(platform->user_data_f32_1, platform->user_data_f32_2);
        platform->velocity.x *= -1.0f;
    }

    if (MAX(platform->user_data_f32_1, platform->user_data_f32_2) <= platform->col.pos.x) {
        platform->col.pos.x = MAX(platform->user_data_f32_1, platform->user_data_f32_2);
        platform->velocity.x *= -1.0f;
    }

    auto list = platform_entity_lists.get(platform->id);
    if (!list.has_value)       return;
    if (list.value->size == 0) return;

    // FIXME: HACK!!! This hack is here because when we move the entity that is standing on the platform, it considers friction which makes it slowly slide off the platform.
    f32 saved_friction = platform->friction_when_tangible;
    platform->friction_when_tangible = 0.0f;
    for (i32 i = 0; i < list.value->capacity; ++i) {
        if (!Ichigo::entity_is_dead(list.value->data[i])) {
            Ichigo::Entity *e        = Ichigo::get_entity(list.value->data[i]);
            Vec2<f32> saved_velocity = e->velocity;
            e->velocity.x            = velocity_before;

            Ichigo::move_entity_in_world(e);

            e->velocity = saved_velocity;
        }
    }

    platform->friction_when_tangible = saved_friction;
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
    platform_spritesheet_texture = Ichigo::load_texture(platform_spritesheet_png, platform_spritesheet_png_len);
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
