#include "../ichigo.hpp"

void Ichigo::Game::init() {
    // Ichigo::load_tilemap(tilemap);
    Ichigo::player_entity = {
        {{3.0f, 5.0f}, 1.0f, 1.0f},
        {0.0f, 0.0f},
        {0.0f, 0.0f},
        Ichigo::IT_PLAYER,
        0,
        nullptr,
        Ichigo::EntityControllers::player_controller
    };

    // Ichigo::set_camera_mode(Ichigo::Camera::FOLLOW);
    // Ichigo::Camera::follow_camera_target = player.entity_id;
}

void Ichigo::Game::frame_begin() {
    // Ichigo::dt;
}

void Ichigo::Game::update_and_render() {


    // Ichigo::push_draw_command(...);
}

void Ichigo::Game::frame_end() {

}