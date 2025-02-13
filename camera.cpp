/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Camera functions.

    Author:      Braeden Hong
    Last edited: 2024/11/30
*/

#include "entity.hpp"
#include "ichigo.hpp"

// TODO: Remove offset and only use the transform.
Vec2<f32> Ichigo::Camera::offset{};
Mat4<f32> Ichigo::Camera::transform{};
Ichigo::Camera::Mode Ichigo::Camera::mode{};

static Ichigo::EntityID follow_target{};
Vec2<f32> Ichigo::Camera::manual_focus_point{};
Vec2<f32> Ichigo::Camera::screen_tile_dimensions{};

// Follow some entity by centering the camera on it.
void Ichigo::Camera::follow(Ichigo::EntityID entity_id) {
    Entity *entity;
    if (!(entity = Ichigo::get_entity(entity_id))) {
        ICHIGO_ERROR("Camera asked to follow non-existant entity!");
        return;
    }

    follow_target = entity_id;
    ICHIGO_INFO("Camera now following: %s (%s)", entity->name, Ichigo::Internal::entity_id_as_string(entity_id));
}

// Called every frame to update the camera's transform.
void Ichigo::Camera::update() {
    switch (mode) {
        case MANUAL: {
            offset = {
                manual_focus_point.x,
                manual_focus_point.y
            };
            transform = translate2d({-offset.x, -offset.y});
        } break;

        case FOLLOW: {
            Entity *following = get_entity(follow_target);
            if (!following) {
                ICHIGO_ERROR("Camera's follow target was killed or was never set!");
                offset = {0.0f, 0.0f};
                transform = m4identity();
            } else {

                offset = {
                    clamp(following->col.pos.x - (Camera::screen_tile_dimensions.x / 2.0f), 0.0f, (f32) Internal::current_tilemap.width - Camera::screen_tile_dimensions.x),
                    clamp(following->col.pos.y - (Camera::screen_tile_dimensions.y / 2.0f), 0.0f, (f32) Internal::current_tilemap.height - Camera::screen_tile_dimensions.y)
                };
                transform = translate2d({-offset.x, -offset.y});

                Rect<f32> camera_rect = {offset, screen_tile_dimensions.x, screen_tile_dimensions.y};
                for (i64 i = 0; i < Internal::entities.size; ++i) {
                    const Entity &e = Internal::entities[i];
                    if (FLAG_IS_SET(e.flags, EF_BLOCKS_CAMERA) && rectangles_intersect(camera_rect, e.col)) {
                        // FIXME: Stupid?
                        // TODO: Block in the y axis as well
                        if (following->col.pos.x < e.col.pos.x + e.col.w) {
                            offset.x = clamp(offset.x, 0.0f, e.col.pos.x - screen_tile_dimensions.x);
                        } else {
                            offset.x = clamp(offset.x, e.col.pos.x + e.col.w, (f32) Internal::current_tilemap.width - Camera::screen_tile_dimensions.x);
                        }

                        transform   = translate2d({-offset.x, -offset.y});
                        camera_rect = {offset, screen_tile_dimensions.x, screen_tile_dimensions.y};
                    }
                }
            }
        } break;
    }
}
