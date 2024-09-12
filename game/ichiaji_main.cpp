#include "../ichigo.hpp"

void Ichigo::game_init() {
    Ichigo::load_tilemap(tilemap);
    Ichigo::spawn_entity(player, Vec2<f32>{0, 0});
    Ichigo::set_camera_mode(Ichigo::Camera::FOLLOW);
    Ichigo::Camera::follow_camera_set_target(player);
}

void Ichigo::frame_begin() {
    Ichigo::dt;
}

void Ichigo::game_update_and_render() {
    Ichigo::push_render_command(...);
}

void Ichigo::frame_end() {

}