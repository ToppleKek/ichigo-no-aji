/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Entity functions.

    Author:      Braeden Hong
    Last edited: 2024/11/25
*/

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
    ret.id = {0, (u32) Internal::entities.size};
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

void Ichigo::conduct_end_of_frame_executions() {
    for (u32 i = 1; i < Internal::entities.size; ++i) {
        Entity &entity = Internal::entities.at(i);
        if (entity.id.index != 0 && FLAG_IS_SET(entity.flags, EF_MARKED_FOR_DEATH)) {
            entity.id.index = 0;
            CLEAR_FLAG(entity.flags, EF_MARKED_FOR_DEATH);
        }
    }
}

void Ichigo::kill_entity(EntityID id) {
    Ichigo::Entity *entity = Ichigo::get_entity(id);
    if (!entity) {
        ICHIGO_ERROR("Tried to kill a non-existant entity!");
        return;
    }

    entity->id.index = 0;
}

void Ichigo::kill_entity_deferred(EntityID id) {
    Ichigo::Entity *entity = Ichigo::get_entity(id);
    if (!entity) {
        ICHIGO_ERROR("Tried to kill a non-existant entity!");
        return;
    }

    SET_FLAG(entity->flags, EF_MARKED_FOR_DEATH);
}

// Perform a sort of "reverse lerp" to find the time, "best_t", at which the dx vector would collide with the wall at x.
// Ensure that this collision is valid by verifying that the y value, "py" ("player y"), would actually hit this wall: ty0 and ty1 ("tile y 0" and "tile y 1").
// x = a + t*(b-a)
static bool test_wall(f32 x, f32 x0, f32 dx, f32 py, f32 dy, f32 ty0, f32 ty1, f32 *best_t) {
    // SEARCH IN T (x - x_0)/(x_1 - x_0) = t
    f32 t = safe_ratio_1(x - x0, dx);
    f32 y = t * dy + py;

// TODO: Is this ok?
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
    // FIXME: maybe this should consider friction...?
    Vec2<f32> entity_delta = 0.5f * entity->acceleration * (Ichigo::Internal::dt * Ichigo::Internal::dt) + entity->velocity * Ichigo::Internal::dt;
    return entity_delta + entity->col.pos;
}

// Move the entity in the world, considering all external forces (gravity, friction) and performing all collision detection (other entities and tilemap).
Ichigo::EntityMoveResult Ichigo::move_entity_in_world(Ichigo::Entity *entity) {
    EntityMoveResult result = NOTHING_SPECIAL;

    // The friction used in all calculations will be the tile with the highest friction.
    const TileInfo &left_standing_tile_info  = Internal::current_tilemap.tile_info[tile_at(entity->left_standing_tile)];
    const TileInfo &right_standing_tile_info = Internal::current_tilemap.tile_info[tile_at(entity->right_standing_tile)];
    f32 friction = MAX(left_standing_tile_info.friction, right_standing_tile_info.friction);

    Vec2<f32> external_acceleration = {};
    i32 direction = (i32) signof(entity->velocity.x);

    // Traction.
    if (FLAG_IS_SET(entity->flags, EF_ON_GROUND) && entity->acceleration.x != 0.0f) {
        // On a high friction surface, you would keep a lot of accelreation. On a low friction surface, you would lose a lot of your acceleration.
        external_acceleration.x = entity->acceleration.x + (std::fabsf(entity->acceleration.x) * (safe_ratio_0(1.0f, safe_ratio_1(friction, 2.0f)) * -signof(entity->acceleration.x)));
    } else if (!FLAG_IS_SET(entity->flags, EF_ON_GROUND) && entity->acceleration.x != 0.0f) {
        // Drag/air resistance
        // TODO: Make drag configurable.
        external_acceleration.x += -6.0f * signof(entity->velocity.x);
    }

    // Friction deceleration. Only applies when you are not trying to move.
    // NOTE: Kind of confusing. entity->acceleration is ONLY the "applied acceleration" by the entity. Does NOT include the external acceleration values.
    if (FLAG_IS_SET(entity->flags, EF_ON_GROUND) && entity->acceleration.x == 0.0f && entity->velocity.x != 0.0f) {
        external_acceleration.x = -friction * direction;
    }

    // Gravity.
    if (!FLAG_IS_SET(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
        external_acceleration.y = entity->gravity;
    }

    Vec2<f32> final_acceleration = {};

    // If the entity is above its maximum velocity, do not apply any of the entity's acceleration, and only apply external forces.
    // This allows the entity to naturally slow down to the max velocity instead of immediately getting clamped to the max.
    bool was_limited = false;
    if (std::fabsf(entity->velocity.x) > entity->max_velocity.x) {
        // FIXME: Stupid hack. Since the entity is likely still applying acceleration, we forcibly apply friction here.
        if (FLAG_IS_SET(entity->flags, EF_ON_GROUND)) {
            external_acceleration.x = -friction * direction;
        }

        final_acceleration.x = external_acceleration.x;
        if (signof(entity->acceleration.x) != signof(entity->velocity.x)) {
            final_acceleration.x += entity->acceleration.x;
        }

        was_limited = true;
    } else {
        final_acceleration.x = external_acceleration.x + entity->acceleration.x;
    }

    final_acceleration.y = entity->acceleration.y + external_acceleration.y;
    // entity->acceleration = final_acceleration;

    // Equations of motion!
    // p' = 1/2 at^2 + vt + p
    // v' = at + v
    Vec2<f32> entity_delta = 0.5f * final_acceleration * (Ichigo::Internal::dt * Ichigo::Internal::dt) + entity->velocity * Ichigo::Internal::dt;
    Rect<f32> potential_next_col = entity->col;
    potential_next_col.pos = entity_delta + entity->col.pos;

    if (entity->velocity.x != 0.0f) {
        entity->velocity += final_acceleration * Ichigo::Internal::dt;
        // Stop the velocity at 0 before changing direction. This prevents the friction force from pushing back infinitely.
        if (signof(entity->velocity.x) != direction) {
            entity->velocity.x = 0.0f;
        }
    } else {
        entity->velocity += final_acceleration * Ichigo::Internal::dt;
    }

    // Should the entity's velocity actually be clamped? If the entity is approaching from below the maximum velocity, then yes.
    // If the entity is decelerating, then only if they just fell below the max.
    bool should_clamp_due_to_decel = was_limited && std::fabsf(entity->velocity.x) < entity->max_velocity.x && (entity->acceleration.x != 0.0f && signof(entity->acceleration.x) == signof(entity->velocity.x));
    bool should_clamp_due_to_accel = !was_limited && std::fabsf(entity->velocity.x) > entity->max_velocity.x;

    if (should_clamp_due_to_decel || should_clamp_due_to_accel) {
        entity->velocity.x = entity->max_velocity.x * signof(entity->velocity.x);
    }

    // entity->velocity.x = clamp(entity->velocity.x, -entity->max_velocity.x, entity->max_velocity.x);
    entity->velocity.y = clamp(entity->velocity.y, -entity->max_velocity.y, entity->max_velocity.y);

    // if (!FLAG_IS_SET(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
    //     entity->velocity.y += entity->gravity * Ichigo::Internal::dt;
    // }

    if (entity->velocity.x == 0.0f && entity->velocity.y == 0.0f) {
        return NO_MOVE;
    }

    // Check entity collisions.
    // Do not check dead entities, or entities that are not moving (since the detection algorithm depends on checking if one collider is moving into another).
    if (entity->id.index != 0 || entity->velocity != Vec2<f32>{0.0f, 0.0f}) {
        Vec2<f32> centered_entity_p = entity->col.pos + Vec2<f32>{entity->col.w / 2.0f, entity->col.h / 2.0f};
        f32 best_t = 1.0f;

        for (u32 i = 1; i < Ichigo::Internal::entities.size; ++i) {
            Ichigo::Entity &other_entity = Ichigo::Internal::entities.at(i);

            // Do not check against ourselves or dead entites.
            if (other_entity.id == entity->id || other_entity.id.index == 0) {
                continue;
            }

            // Skip this entity if the colliders already intersect. We only run the collide procedure on collider enter.
            // TODO: Call the collide procedure on exit as well? We would just have to mirror the normal vector.
            //       Or maybe have a separate callback for when they are already intersecting?
            if (rectangles_intersect(entity->col, other_entity.col)) {
                continue;
            }

            Vec2<f32> min_corner = {other_entity.col.pos.x - entity->col.w / 2.0f, other_entity.col.pos.y - entity->col.h / 2.0f};
            Vec2<f32> max_corner = {other_entity.col.pos.x + other_entity.col.w + entity->col.w / 2.0f, other_entity.col.pos.y + other_entity.col.h + entity->col.h / 2.0f};
            Vec2<f32> normal = {};

            if (test_wall(min_corner.x, centered_entity_p.x, entity_delta.x, centered_entity_p.y, entity_delta.y, min_corner.y, max_corner.y, &best_t)) {
                normal = { -1, 0 };
            }
            if (test_wall(max_corner.x, centered_entity_p.x, entity_delta.x, centered_entity_p.y, entity_delta.y, min_corner.y, max_corner.y, &best_t)) {
                normal = { 1, 0 };
            }
            if (test_wall(min_corner.y, centered_entity_p.y, entity_delta.y, centered_entity_p.x, entity_delta.x, min_corner.x, max_corner.x, &best_t)) {
                normal = { 0, -1 };
            }
            if (test_wall(max_corner.y, centered_entity_p.y, entity_delta.y, centered_entity_p.x, entity_delta.x, min_corner.x, max_corner.x, &best_t)) {
                normal = { 0, 1 };
            }

            if (normal.x != 0.0f || normal.y != 0.0f) {
                // We guarantee that the first entity parameter is can always be considered the "self" entity.
                if (entity->collide_proc) {
                    entity->collide_proc(entity, &other_entity, normal, entity->col.pos + entity_delta * best_t);
                }

                if (other_entity.collide_proc) {
                    other_entity.collide_proc(&other_entity, entity, normal * Vec2<f32>{-1.0f, -1.0f}, entity->col.pos + entity_delta * best_t);
                }
            }
        }
    }

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
                const TileInfo &tile_info = Internal::current_tilemap.tile_info[Ichigo::tile_at({tile_x, tile_y})];
                if (FLAG_IS_SET(tile_info.flags, TileFlag::TANGIBLE)) {
                    Vec2<f32> centered_entity_p = entity->col.pos + Vec2<f32>{entity->col.w / 2.0f, entity->col.h / 2.0f};
                    Vec2<f32> min_corner = {tile_x - entity->col.w / 2.0f, tile_y - entity->col.h / 2.0f};
                    Vec2<f32> max_corner = {tile_x + 1 + entity->col.w / 2.0f, tile_y + 1 + entity->col.h / 2.0f};
                    // ICHIGO_INFO("min_corner=%f,%f max_corner=%f,%f tile=%u,%u", min_corner.x, min_corner.y, max_corner.x, max_corner.y, tile_x, tile_y);
                    if (test_wall(min_corner.x, centered_entity_p.x, entity_delta.x, centered_entity_p.y, entity_delta.y, min_corner.y, max_corner.y, &best_t)) {
                        wall_normal = { -1, 0 };
                        wall_position = { (f32) tile_x, (f32) tile_y };
                        // ICHIGO_INFO("(-1,0) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }
                    if (test_wall(max_corner.x, centered_entity_p.x, entity_delta.x, centered_entity_p.y, entity_delta.y, min_corner.y, max_corner.y, &best_t)) {
                        wall_normal = { 1, 0 };
                        wall_position = { (f32) tile_x, (f32) tile_y };
                        // ICHIGO_INFO("(1,0) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }
                    if (test_wall(min_corner.y, centered_entity_p.y, entity_delta.y, centered_entity_p.x, entity_delta.x, min_corner.x, max_corner.x, &best_t)) {
                        wall_normal = { 0, -1 };
                        wall_position = { (f32) tile_x, (f32) tile_y };
                        // ICHIGO_INFO("(0,-1) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }
                    if (test_wall(max_corner.y, centered_entity_p.y, entity_delta.y, centered_entity_p.x, entity_delta.x, min_corner.x, max_corner.x, &best_t)) {
                        wall_normal = { 0, 1 };
                        wall_position = { (f32) tile_x, (f32) tile_y };
                        // ICHIGO_INFO("(0,1) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }
                }
            }
        }

        if (wall_normal.x != 0.0f) {
            result = HIT_WALL;
        } else if (wall_normal.y == 1.0f) {
            result = HIT_CEILING;
        }

        if (!FLAG_IS_SET(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND) && wall_normal.y == -1.0f) {
            result = HIT_GROUND;
            SET_FLAG(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);
            // TODO: Bug where you stop at 0.0000001 units away from the ground
            //       Floating point precision error? Maybe we will just snap to the floor when we touch it?
            //       This has implications based on what kind of floor it is. What if we want a bouncy floor?
            // The bug is that due to floating point imprecision, we end up slightly above the floor, but here since we hit the floor (wall normal y is -1),
            // we are counted as hitting the floor. Next frame, we are standing on air, so we apply gravity. Then, the best_t gets so small
            // that it ends up not moving the player at all (because we are so close to the floor).

            // Solutions:
            // 1: Snap to the floor
            // 2: Round the player's y position if they hit a floor
            //    Can't really do half tiles very well (is that a real problem? -NO!)
            // 3: Epsilon error correction
        }

        Vec2<f32> final_delta{};
#define D_EPSILON 0.0001
        if (wall_normal.x == 1.0f) {
            entity->col.pos.x = wall_position.x + 1.0f + D_EPSILON;
        } else if (wall_normal.x == -1.0f) {
            entity->col.pos.x = wall_position.x - entity->col.w - D_EPSILON;
        } else if (wall_normal.y == 1.0f) {
            entity->col.pos.y = wall_position.y + 1.0f + D_EPSILON;
        } else if (wall_normal.y == -1.0f) {
            entity->col.pos.y = wall_position.y - entity->col.h - D_EPSILON;
        } else {
            final_delta.x = std::fabsf(entity_delta.x * best_t) < D_EPSILON ? 0.0f : entity_delta.x * best_t;
            final_delta.y = std::fabsf(entity_delta.y * best_t) < D_EPSILON ? 0.0f : entity_delta.y * best_t;
            entity->col.pos += final_delta;
#undef  D_EPSILON
        }

        entity->velocity = entity->velocity - 1 * dot(entity->velocity, wall_normal) * wall_normal;
        entity_delta = entity_delta - 1 * dot(entity_delta, wall_normal) * wall_normal;
        t_remaining -= best_t * t_remaining;

        // for (u32 tile_y = min_tile_y; tile_y <= max_tile_y; ++tile_y) {
        //     for (u32 tile_x = min_tile_x; tile_x <= max_tile_x; ++tile_x) {
        //         if (tile_at({tile_x, tile_y}) != 0 && rectangles_intersect({{(f32) tile_x, (f32) tile_y}, 1.0f, 1.0f}, entity->col)) {
        //             ICHIGO_ERROR(
        //                 "Collision fail at tile %d,%d! potential_next_col=%f,%f wall_normal=%f,%f entity %s position now=%f,%f",
        //                 tile_x, tile_y, potential_next_col.pos.x, potential_next_col.pos.y, wall_normal.x, wall_normal.y, Internal::entity_id_as_string(entity->id), entity->col.pos.x, entity->col.pos.y
        //             );
        //             __builtin_debugtrap();
        //         }
        //     }
        // }

        // if (entity->velocity.x == 0.0f && entity->velocity.y == 0.0f) {
        //     ICHIGO_INFO("EARLY OUT");
        //     break;
        // }
    }

    // If an entity becomes airborne on its own (eg. jumping)
    if (entity->velocity.y != 0.0f && FLAG_IS_SET(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
        result = BECAME_AIRBORNE;
        CLEAR_FLAG(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);
    }

    // Check if the entity is standing on anything. If they aren't anymore, mark them as airborne.
    if (FLAG_IS_SET(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
        entity->left_standing_tile  = { (u32) entity->col.pos.x, (u32) (entity->col.pos.y + entity->col.h) + 1 };
        entity->right_standing_tile = { (u32) (entity->col.pos.x + entity->col.w), (u32) (entity->col.pos.y + entity->col.h) + 1 };
        if (Ichigo::tile_at(entity->left_standing_tile) == ICHIGO_AIR_TILE && Ichigo::tile_at(entity->right_standing_tile) == ICHIGO_AIR_TILE) {
            result = BECAME_AIRBORNE;
            CLEAR_FLAG(entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);
        }
    }

    return result;
}

void Ichigo::EntityControllers::player_controller(Ichigo::Entity *player_entity) {
    static f32 jump_t = 0.0f;

    player_entity->acceleration = {0.0f, 0.0f};
    if (Ichigo::Internal::keyboard_state[Ichigo::IK_RIGHT].down) {
        player_entity->acceleration.x = player_entity->movement_speed;
        SET_FLAG(player_entity->flags, Ichigo::EF_FLIP_H);
    }

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT].down) {
        player_entity->acceleration.x = -player_entity->movement_speed;
        CLEAR_FLAG(player_entity->flags, Ichigo::EF_FLIP_H);
    }

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_SPACE].down_this_frame && FLAG_IS_SET(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND))
        jump_t = 0.06f;

    if (jump_t != 0.0f) {
        CLEAR_FLAG(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);
        f32 effective_dt = jump_t < Ichigo::Internal::dt ? jump_t : Ichigo::Internal::dt;
        player_entity->acceleration.y = -player_entity->jump_acceleration * (effective_dt / Ichigo::Internal::dt);
        jump_t -= effective_dt;
    }

    Ichigo::move_entity_in_world(player_entity);
}

void Ichigo::EntityControllers::patrol_controller(Entity *entity) {
    entity->acceleration = {entity->movement_speed * signof(entity->acceleration.x), 0.0f};

    Vec2<f32> projected_next_pos = calculate_projected_next_position(entity);

    Vec2<u32> projected_left_standing_tile  = { (u32) projected_next_pos.x, (u32) (projected_next_pos.y + entity->col.h) + 1 };
    Vec2<u32> projected_right_standing_tile = { (u32) (projected_next_pos.x + entity->col.w), (u32) (projected_next_pos.y + entity->col.h) + 1 };

    const TileInfo &left_info  = Internal::current_tilemap.tile_info[Ichigo::tile_at(projected_left_standing_tile)];
    const TileInfo &right_info = Internal::current_tilemap.tile_info[Ichigo::tile_at(projected_right_standing_tile)];
    if (!FLAG_IS_SET(left_info.flags, TANGIBLE)) {
        entity->acceleration.x = entity->movement_speed;
        entity->velocity.x = 0.0f;
    } else if (!FLAG_IS_SET(right_info.flags, TANGIBLE)) {
        entity->acceleration.x = -entity->movement_speed;
        entity->velocity.x = 0.0f;
    }

    EntityMoveResult move_result = Ichigo::move_entity_in_world(entity);

    if (move_result == HIT_WALL) {
        entity->acceleration.x = -entity->acceleration.x;
    }
}

char *Ichigo::Internal::entity_id_as_string(EntityID entity_id) {
    static char str[22];
    snprintf(str, sizeof(str), "%u:%u", entity_id.generation, entity_id.index);
    return str;
}
