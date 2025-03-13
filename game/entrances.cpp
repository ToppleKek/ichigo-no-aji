#include "entrances.hpp"
#include "ichiaji.hpp"

#include <immintrin.h>

EMBED("assets/key.png", key_png)
EMBED("assets/lock.png", lock_png)
EMBED("assets/entrance.png", entrance_png)

static Ichigo::TextureID key_texture;
static Ichigo::TextureID lock_texture;
static Ichigo::TextureID entrance_texture;

struct LockedDoor {
    u64 unlock_flags;
    Vec2<f32> exit_location;
    u8 unlocked_bit;
};

static Bana::BucketArray<LockedDoor> locked_doors;

static void render_locked_door(Ichigo::Entity *e) {
    Bana::BucketLocator bl = *((Bana::BucketLocator *) &e->user_data_i64);
    LockedDoor &ld         = locked_doors[bl];

    Ichigo::world_render_textured_rect(e->col, entrance_texture);


    if (!FLAG_IS_SET(Ichiaji::current_level_save_data().progress_flags, 1 << ld.unlocked_bit)) {
        Ichigo::Texture &lock_tex = Ichigo::Internal::textures[lock_texture];
        // FIXME: position this.
        Rect<f32> r = {e->col.pos + Vec2<f32>{e->col.w / 2.0f, e->col.h / 2.0f}, pixels_to_metres(lock_tex.width), pixels_to_metres(lock_tex.height)};
        Ichigo::world_render_textured_rect(r, lock_texture);
    }
}

static void kill_locked_door(Ichigo::Entity *e) {
    Bana::BucketLocator bl = *((Bana::BucketLocator *) (&e->user_data_i64));
    locked_doors.remove(bl);
}

static void on_key_collide(Ichigo::Entity *key, Ichigo::Entity *other, [[maybe_unused]] Vec2<f32> collider_normal, [[maybe_unused]] Vec2<f32> collision_normal, [[maybe_unused]] Vec2<f32> collision_pos) {
    if (other->user_type_id == ET_PLAYER) {
        SET_FLAG(Ichiaji::current_level_save_data().progress_flags, key->user_data_i64);
        Ichigo::show_info("Key get!");
        Ichigo::kill_entity_deferred(key);
    }
}

void Entrances::init() {
    key_texture      = Ichigo::load_texture(key_png, key_png_len);
    lock_texture     = Ichigo::load_texture(lock_png, lock_png_len);
    entrance_texture = Ichigo::load_texture(entrance_png, entrance_png_len);

    locked_doors = make_bucket_array<LockedDoor>(128, Ichigo::Internal::perm_allocator);
}

void Entrances::spawn_entrance(const Ichigo::EntityDescriptor &descriptor) {
    Ichigo::Entity *entrance = Ichigo::spawn_entity();
    Ichigo::Texture &tex     = Ichigo::Internal::textures[entrance_texture];

    std::strcpy(entrance->name, "entrance");

    entrance->col                                  = {descriptor.pos, 1.0f, 1.0f};
    entrance->sprite.width                         = 1.0f;
    entrance->sprite.height                        = 1.0f;
    entrance->sprite.sheet.texture                 = entrance_texture;
    entrance->sprite.sheet.cell_width              = pixels_to_metres(tex.width);
    entrance->sprite.sheet.cell_height             = pixels_to_metres(tex.height);
    entrance->sprite.animation                     = {};
    entrance->user_data_i64                        = descriptor.data;
    entrance->user_type_id                         = descriptor.type; // NOTE: Could be an ET_ENTRANCE or an ET_LEVEL_ENTRANCE.

    SET_FLAG(entrance->flags, Ichigo::EF_STATIC);
}

void Entrances::spawn_entrance_trigger(const Ichigo::EntityDescriptor &descriptor) {
    Ichigo::Entity *entrance = Ichigo::spawn_entity();

    std::strcpy(entrance->name, "entrance_trigger");

    entrance->col           = {descriptor.pos, descriptor.custom_width, descriptor.custom_height};
    entrance->user_data_i64 = descriptor.data;
    entrance->user_type_id  = descriptor.type;

    SET_FLAG(entrance->flags, Ichigo::EF_INVISIBLE);
    SET_FLAG(entrance->flags, Ichigo::EF_STATIC);

    if (descriptor.type == ET_ENTRANCE_TRIGGER_H) {
        SET_FLAG(entrance->flags, Ichigo::EF_BLOCKS_CAMERA_Y);
    } else {
        SET_FLAG(entrance->flags, Ichigo::EF_BLOCKS_CAMERA_X);
    }
}

void Entrances::spawn_death_trigger(const Ichigo::EntityDescriptor &descriptor) {
    Ichigo::Entity *death_trigger = Ichigo::spawn_entity();

    std::strcpy(death_trigger->name, "death_trigger");

    death_trigger->col           = {descriptor.pos, descriptor.custom_width, descriptor.custom_height};
    death_trigger->user_type_id  = ET_DEATH_TRIGGER;

    SET_FLAG(death_trigger->flags, Ichigo::EF_INVISIBLE);
    SET_FLAG(death_trigger->flags, Ichigo::EF_BLOCKS_CAMERA_Y);
    SET_FLAG(death_trigger->flags, Ichigo::EF_STATIC);
}


void Entrances::spawn_locked_door(const Ichigo::EntityDescriptor &descriptor, u64 unlock_flags, Vec2<f32> exit_location, u8 unlocked_bit) {
    LockedDoor ld          = {unlock_flags, exit_location, unlocked_bit};
    Bana::BucketLocator bl = locked_doors.insert(ld);
    Ichigo::Texture &tex   = Ichigo::Internal::textures[entrance_texture];
    Ichigo::Entity *e      = Ichigo::spawn_entity();

    std::strcpy(e->name, "locked_door");

    e->col           = {descriptor.pos, pixels_to_metres(tex.width), pixels_to_metres(tex.height)};
    e->render_proc   = render_locked_door;
    e->kill_proc     = kill_locked_door;
    e->user_data_i64 = *((i64 *) &bl);
    e->user_type_id  = ET_LOCKED_DOOR;

    SET_FLAG(e->flags, Ichigo::EF_STATIC);
}

void Entrances::spawn_key(const Ichigo::EntityDescriptor &descriptor) {
    Ichigo::Entity *e      = Ichigo::spawn_entity();
    Ichigo::Texture &tex   = Ichigo::Internal::textures[key_texture];

    static const Ichigo::Sprite key_sprite = {
        .pos_offset = {},
        .width      = pixels_to_metres(tex.width),
        .height     = pixels_to_metres(tex.height),
        .sheet      = {
            .cell_width  = tex.width,
            .cell_height = tex.height,
            .texture     = key_texture,
        },
        .animation                    = {},
        .current_animation_frame      = 0,
        .elapsed_animation_frame_time = 0.0f
    };

    std::strcpy(e->name, "key");

    e->col           = {descriptor.pos, pixels_to_metres(tex.width), pixels_to_metres(tex.height)};
    e->sprite        = key_sprite;
    e->collide_proc  = on_key_collide;
    e->user_data_i64 = descriptor.data;
    e->user_type_id  = ET_KEY;

    SET_FLAG(e->flags, Ichigo::EF_STATIC);
}

void Entrances::spawn_shop_entrance(const Ichigo::EntityDescriptor &descriptor) {
    Ichigo::Texture &tex   = Ichigo::Internal::textures[entrance_texture];
    Ichigo::Entity *e      = Ichigo::spawn_entity();

    static const Ichigo::Sprite shop_entrance_sprite = {
        .pos_offset = {},
        .width      = pixels_to_metres(tex.width),
        .height     = pixels_to_metres(tex.height),
        .sheet      = {
            .cell_width  = tex.width,
            .cell_height = tex.height,
            .texture     = entrance_texture,
        },
        .animation                    = {},
        .current_animation_frame      = 0,
        .elapsed_animation_frame_time = 0.0f
    };

    std::strcpy(e->name, "shop_entrance");

    e->col           = {descriptor.pos, pixels_to_metres(tex.width), pixels_to_metres(tex.height)};
    e->sprite        = shop_entrance_sprite;
    e->user_type_id  = ET_SHOP_ENTRANCE;

    SET_FLAG(e->flags, Ichigo::EF_STATIC);
}

Bana::Optional<Vec2<f32>> Entrances::get_exit_location_if_possible(Ichigo::EntityID eid) {
    Ichigo::Entity *e = Ichigo::get_entity(eid);
    if (!e) return {};

    if (e->user_type_id == ET_LOCKED_DOOR) {
        Bana::BucketLocator bl = *((Bana::BucketLocator *) &e->user_data_i64);
        LockedDoor &ld         = locked_doors[bl];

        u64 total_keys_required = _mm_popcnt_u64(ld.unlock_flags);
        u64 keys_collected      = _mm_popcnt_u64(Ichiaji::current_level_save_data().progress_flags & ld.unlock_flags);
        if (total_keys_required != keys_collected) {
            Bana::String s = make_string(64, Ichigo::Internal::temp_allocator);
            Bana::string_format(s, "You need %llu more keys to open this door.", total_keys_required - keys_collected);
            Ichigo::show_info(s.data, s.length);
            return {};
        }

        SET_FLAG(Ichiaji::current_level_save_data().progress_flags, 1 << ld.unlocked_bit);
        return ld.exit_location;
    }

    return {};
}
