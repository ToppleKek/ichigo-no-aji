#include "editor.hpp"
#include "ichigo.hpp"
#include "thirdparty/imgui/imgui.h"

void Ichigo::Editor::render_ui() {
    ImGui::SetNextWindowPos({Ichigo::Internal::window_width * 0.8f, 0.0f});
    ImGui::SetNextWindowSize({Ichigo::Internal::window_width * 0.2f, (f32) Ichigo::Internal::window_height});
    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Tilemap size: %ux%u", Ichigo::Internal::current_tilemap_width, Ichigo::Internal::current_tilemap_height);
    ImGui::End();
}

#define BASE_CAMERA_SPEED 10.0f

void Ichigo::Editor::update() {
    f32 camera_speed = BASE_CAMERA_SPEED;
    if (Internal::keyboard_state[IK_LEFT_SHIFT].down)
        camera_speed *= 2.0f;

    if (Internal::keyboard_state[IK_W].down)
        Ichigo::Camera::manual_focus_point.y -= camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_S].down)
        Ichigo::Camera::manual_focus_point.y += camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_A].down)
        Ichigo::Camera::manual_focus_point.x -= camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_D].down)
        Ichigo::Camera::manual_focus_point.x += camera_speed * Ichigo::Internal::dt;
}