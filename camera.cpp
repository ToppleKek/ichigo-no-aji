#include "entity.hpp"
#include "ichigo.hpp"

Vec2<f32> Ichigo::Camera::offset{};
Mat4<f32> Ichigo::Camera::transform{};

static Ichigo::EntityID follow_target{};

void Ichigo::Camera::follow(Ichigo::EntityID entity_id) {
    Entity *entity;
    if (!(entity = Ichigo::get_entity(entity_id))) {
        ICHIGO_ERROR("Camera asked to follow non-existant entity!");
        return;
    }

    follow_target = entity_id;
    ICHIGO_INFO("Camera now following: %s (%s)", entity->name, Ichigo::Internal::entity_id_as_string(entity_id));
}

void Ichigo::Camera::update() {
    Entity *following = get_entity(follow_target);
    if (!following) {
        ICHIGO_ERROR("Camera's follow target was killed or was never set!");
        offset = {0.0f, 0.0f};
        transform = m4identity();
    } else {
        offset = {clamp(following->col.pos.x - (SCREEN_TILE_WIDTH / 2.0f), 0.0f, (f32) Ichigo::Internal::current_tilemap_width - SCREEN_TILE_WIDTH), clamp(following->col.pos.y - (SCREEN_TILE_HEIGHT / 2.0f), 0.0f, (f32) Ichigo::Internal::current_tilemap_height - SCREEN_TILE_HEIGHT)};
        transform = translate2d({-offset.x, -offset.y});
    }
}
