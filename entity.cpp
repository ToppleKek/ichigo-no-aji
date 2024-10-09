#include "entity.hpp"
#include "ichigo.hpp"

Util::IchigoVector<Ichigo::Entity> Ichigo::Internal::entities{512};

// TODO: If the entity id's index is 0, that means that entity has been killed and that spot can hold a new entity.
//       Should it really be like this? Maybe there should be an 'is_alive' flag on the entity?

Ichigo::Entity *Ichigo::spawn_entity() {
    // Search for an open entity slot
    for (u32 i = 1; i < Internal::entities.size; ++i) {
        if (Internal::entities.at(i).id.index == 0) {
            Ichigo::EntityID new_id = {Internal::entities.at(i).id.generation + 1, i};
            std::memset(&Internal::entities.at(i), 0, sizeof(Ichigo::Entity));
            Internal::entities.at(i).id = new_id;
            return &Internal::entities.at(i);
        }
    }

    // If no slots are available, append the entity to the end of the entity list
    Ichigo::Entity ret{};
    ret.id = {0, (u32) Internal::entities.size };
    Internal::entities.append(ret);
    return &Internal::entities.at(Internal::entities.size - 1);
}

Ichigo::Entity *Ichigo::get_entity(Ichigo::EntityID id) {
    if (id.index < 1 || id.index >= Internal::entities.size)
        return nullptr;

    if (Internal::entities.at(id.index).id.generation != id.generation)
        return nullptr;

    return &Internal::entities.at(id.index);
}

void Ichigo::kill_entity(EntityID id) {
    Ichigo::Entity *entity = Ichigo::get_entity(id);
    if (!entity) {
        ICHIGO_ERROR("Tried to kill a non-existant entity!");
        return;
    }

    entity->id.index = 0;
}

// x = a + t*(b-a)
static bool test_wall(f32 x, f32 x0, f32 dx, f32 py, f32 dy, f32 ty0, f32 ty1, f32 *best_t) {
    // SEARCH IN T (x - x_0)/(x_1 - x_0) = t
    f32 t = safe_ratio_1(x - x0, dx);
    f32 y = t * dy + py;

// TODO: Error value?
#define T_EPSILON 0.00999f
    if (t >= 0 && t < *best_t) {
        if ((y > ty0 && y < ty1)) {
            *best_t = MAX(0.0f, t - T_EPSILON);
#undef T_EPSILON
            return true;
        } else {
            // ICHIGO_INFO("y test failed t=%f x=%f x0=%f dx=%f py=%f dy=%f ty0=%f ty1=%f", t, x, x0, dx, py, dy, ty0, ty1);
        }
    }

    return false;
}

static inline Vec2<f32> calculate_projected_next_position(Ichigo::Entity *entity) {
    Vec2<f32> entity_delta = 0.5f * entity->acceleration * (Ichigo::Internal::dt * Ichigo::Internal::dt) + entity->velocity * Ichigo::Internal::dt;
    return entity_delta + entity->col.pos;
}

void Ichigo::move_entity_in_world(Ichigo::Entity *entity) {
    Vec2<f32> entity_delta = 0.5f * entity->acceleration * (Ichigo::Internal::dt * Ichigo::Internal::dt) + entity->velocity * Ichigo::Internal::dt;
    Rectangle potential_next_col = entity->col;
    potential_next_col.pos = entity_delta + entity->col.pos;

    entity->velocity += entity->acceleration * Ichigo::Internal::dt;
    entity->velocity.x = clamp(entity->velocity.x, -entity->max_velocity.x, entity->max_velocity.x);
    entity->velocity.y = clamp(entity->velocity.y, -entity->max_velocity.y, entity->max_velocity.y);

    if (!FLAG_IS_SET(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
        entity->velocity.y += entity->gravity * Ichigo::Internal::dt;
    }

    if (entity->velocity.x == 0.0f && entity->velocity.y == 0.0f) {
        return;
    }

    // ICHIGO_INFO("Nearby tiles this frame:");
    f32 t_remaining = 1.0f;

    for (u32 i = 0; i < 4 && t_remaining > 0.0f; ++i) {
        u32 max_tile_y = std::ceil(MAX(potential_next_col.pos.y + entity->col.h, entity->col.pos.y + entity->col.h));
        u32 max_tile_x = std::ceil(MAX(potential_next_col.pos.x + entity->col.w, entity->col.pos.x + entity->col.w));
        u32 min_tile_y = MIN(potential_next_col.pos.y, entity->col.pos.y);
        u32 min_tile_x = MIN(potential_next_col.pos.x, entity->col.pos.x);
        f32 best_t = 1.0f;
        Vec2<f32> wall_normal{};
        Vec2<f32> wall_position{};

        for (u32 tile_y = min_tile_y; tile_y <= max_tile_y; ++tile_y) {
            for (u32 tile_x = min_tile_x; tile_x <= max_tile_x; ++tile_x) {
                if (Ichigo::tile_at({tile_x, tile_y}) != 0) {
                    Vec2<f32> centered_entity_p = entity->col.pos + Vec2<f32>{entity->col.w / 2.0f, entity->col.h / 2.0f};
                    Vec2<f32> min_corner = {tile_x - entity->col.w / 2.0f, tile_y - entity->col.h / 2.0f};
                    Vec2<f32> max_corner = {tile_x + 1 + entity->col.w / 2.0f, tile_y + 1 + entity->col.h / 2.0f};
                    // ICHIGO_INFO("min_corner=%f,%f max_corner=%f,%f tile=%u,%u", min_corner.x, min_corner.y, max_corner.x, max_corner.y, tile_x, tile_y);
                    bool updated = false;
                    if (test_wall(min_corner.x, centered_entity_p.x, entity_delta.x, centered_entity_p.y, entity_delta.y, min_corner.y, max_corner.y, &best_t)) {
                        updated = true;
                        wall_normal = { -1, 0 };
                        wall_position = { (f32) tile_x, (f32) tile_y };
                        // ICHIGO_INFO("(-1,0) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }
                    if (test_wall(max_corner.x, centered_entity_p.x, entity_delta.x, centered_entity_p.y, entity_delta.y, min_corner.y, max_corner.y, &best_t)) {
                        updated = true;
                        wall_normal = { 1, 0 };
                        wall_position = { (f32) tile_x, (f32) tile_y };
                        // ICHIGO_INFO("(1,0) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }
                    if (test_wall(min_corner.y, centered_entity_p.y, entity_delta.y, centered_entity_p.x, entity_delta.x, min_corner.x, max_corner.x, &best_t)) {
                        updated = true;
                        wall_normal = { 0, -1 };
                        wall_position = { (f32) tile_x, (f32) tile_y };
                        // ICHIGO_INFO("(0,-1) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }
                    if (test_wall(max_corner.y, centered_entity_p.y, entity_delta.y, centered_entity_p.x, entity_delta.x, min_corner.x, max_corner.x, &best_t)) {
                        updated = true;
                        wall_normal = { 0, 1 };
                        wall_position = { (f32) tile_x, (f32) tile_y };
                        // ICHIGO_INFO("(0,1) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }

                    // if (updated)
                    //     ICHIGO_INFO("Decided wall normal: %f,%f", wall_normal.x, wall_normal.y);
                }
            }
        }

        // if (wall_normal.x != 0.0f || wall_normal.y != 0.0f)
        //     ICHIGO_INFO("FINAL wall normal: %f,%f best_t=%f", wall_normal.x, wall_normal.y, best_t);

        if (!FLAG_IS_SET(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND) && wall_normal.y == -1.0f) {
            ICHIGO_INFO("ENTITY %s HIT GROUND at tile: %f,%f", Internal::entity_id_as_string(entity->id), wall_position.x, wall_position.y);
            SET_FLAG(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);
            // TODO: Bug where you stop at 0.0000001 units away from the ground
            //       Floating point precision error? Maybe we will just snap to the floor when we touch it?
            //       This has implications based on what kind of floor it is. What if we want a bouncy floor?
            // The bug is that due to floating point imprecision, we end up slightly above the floor, but here since we hit the floor (wall normal y is -1),
            // we are counted as hitting the floor. Next frame, we are standing on air, so we apply gravity. Then, the best_t gets so small
            // that it ends up not moving the player at all (because we are so close to the floor).

            // Solutions:
            // 1: Snap to the floor
            //    This would probably be the best? We could save the position of the floor that the player hit, and then
            //    determine what to do with the player with that information. If the tile is a bouncy tile, we would bounce the player.
            //    If the tile is a regular floor, snap the player to the floors position.
            // 2: Round the player's y position if they hit a floor
            //    Can't really do half tiles very well (is that a real problem? -NO!)
            // 3: Epsilon error correction?
            // 4: Fix gravity velocity problem?

            // We went with solution 2
            // TODO: This is also a problem where you can get 0.0000002 units into a wall. We can probably lose the last 2 digits of the float and still be happy.
        }

        Vec2<f32> final_delta{};
#define D_EPSILON 0.0001
        final_delta.x = std::fabsf(entity_delta.x * best_t) < D_EPSILON ? 0.0f : entity_delta.x * best_t;
        final_delta.y = std::fabsf(entity_delta.y * best_t) < D_EPSILON ? 0.0f : entity_delta.y * best_t;
#undef  D_EPSILON
        entity->col.pos += final_delta;
        entity->velocity = entity->velocity - 1 * dot(entity->velocity, wall_normal) * wall_normal;
        entity_delta = entity_delta - 1 * dot(entity_delta, wall_normal) * wall_normal;
        t_remaining -= best_t * t_remaining;

        for (u32 tile_y = min_tile_y; tile_y <= max_tile_y; ++tile_y) {
            for (u32 tile_x = min_tile_x; tile_x <= max_tile_x; ++tile_x) {
                if (tile_at({tile_x, tile_y}) != 0 && rectangles_intersect({{(f32) tile_x, (f32) tile_y}, 1.0f, 1.0f}, entity->col)) {
                    ICHIGO_ERROR(
                        "Collision fail at tile %d,%d! potential_next_col=%f,%f wall_normal=%f,%f entity %s position now=%f,%f",
                        tile_x, tile_y, potential_next_col.pos.x, potential_next_col.pos.y, wall_normal.x, wall_normal.y, Internal::entity_id_as_string(entity->id), entity->col.pos.x, entity->col.pos.y
                    );
                    __builtin_debugtrap();
                }
            }
        }

        // if (entity->velocity.x == 0.0f && entity->velocity.y == 0.0f) {
        //     ICHIGO_INFO("EARLY OUT");
        //     break;
        // }
    }

    // if (entity->velocity.y != 0.0f)
    //     CLEAR_FLAG(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);

    if (FLAG_IS_SET(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
        entity->left_standing_tile  = { (u32) entity->col.pos.x, (u32) (entity->col.pos.y + entity->col.h) + 1 };
        entity->right_standing_tile = { (u32) (entity->col.pos.x + entity->col.w), (u32) (entity->col.pos.y + entity->col.h) + 1 };
        if (Ichigo::tile_at(entity->left_standing_tile) == 0 && Ichigo::tile_at(entity->right_standing_tile) == 0) {
            ICHIGO_INFO("ENTITY %s BECAME AIRBORNE!", Internal::entity_id_as_string(entity->id));
            CLEAR_FLAG(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);
        }
    }
}

void Ichigo::EntityControllers::player_controller(Ichigo::Entity *player_entity) {
    static f32 jump_t = 0.0f;

    player_entity->acceleration = {0.0f, 0.0f};
    if (Ichigo::Internal::keyboard_state[Ichigo::IK_RIGHT].down)
        player_entity->acceleration.x = player_entity->movement_speed;
    if (Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT].down)
        player_entity->acceleration.x = -player_entity->movement_speed;
    if (Ichigo::Internal::keyboard_state[Ichigo::IK_SPACE].down_this_frame && FLAG_IS_SET(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND))
        jump_t = 0.06f;

    if (jump_t != 0.0f) {
        CLEAR_FLAG(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);
        f32 effective_dt = jump_t < Ichigo::Internal::dt ? jump_t : Ichigo::Internal::dt;
        player_entity->acceleration.y = -player_entity->jump_acceleration * (effective_dt / Ichigo::Internal::dt);
        jump_t -= effective_dt;
    }

    i32 direction = player_entity->velocity.x < 0 ? -1 : 1;

    if (player_entity->velocity.x != 0.0f) {
        player_entity->velocity += {player_entity->friction * Ichigo::Internal::dt * -direction, 0.0f};
        i32 new_direction = player_entity->velocity.x < 0 ? -1 : 1;

        if (new_direction != direction)
            player_entity->velocity.x = 0.0f;
    }

    // p' = 1/2 at^2 + vt + p
    // v' = at + v
    // a

    Ichigo::move_entity_in_world(player_entity);
}

void Ichigo::EntityControllers::patrol_controller(Entity *entity) {
    if (entity->acceleration.x == 0.0f)
        entity->acceleration.x = -entity->movement_speed;

    Vec2<f32> projected_next_pos = calculate_projected_next_position(entity);

    Vec2<u32> projected_left_standing_tile  = { (u32) projected_next_pos.x, (u32) (projected_next_pos.y + entity->col.h) + 1 };
    Vec2<u32> projected_right_standing_tile = { (u32) (projected_next_pos.x + entity->col.w), (u32) (projected_next_pos.y + entity->col.h) + 1 };

    // FIXME: This is hardcoding 0 as being the only non-solid tile!
    if (Ichigo::tile_at(projected_left_standing_tile) == 0 && Ichigo::tile_at(projected_right_standing_tile) == 0) {
        entity->acceleration.x = -entity->acceleration.x;
        entity->velocity.x = 0.0f;
    }

    Ichigo::move_entity_in_world(entity);

    if (entity->velocity.x == 0.0f)
        entity->acceleration.x = -entity->acceleration.x;
}

char *Ichigo::Internal::entity_id_as_string(EntityID entity_id) {
    static char str[22];
    snprintf(str, sizeof(str), "%u:%u", entity_id.generation, entity_id.index);
    return str;
}
