#include "editor.hpp"
#include "ichigo.hpp"
#include "thirdparty/imgui/imgui.h"

static bool tile_selected      = false;
static Vec2<u32> selected_tile = {};

void Ichigo::Editor::render_ui() {
    static char tilemap_w_text[16];
    static char tilemap_h_text[16];
    ImGui::SetNextWindowPos({Ichigo::Internal::window_width * 0.8f, 0.0f});
    ImGui::SetNextWindowSize({Ichigo::Internal::window_width * 0.2f, (f32) Ichigo::Internal::window_height});
    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    if (ImGui::CollapsingHeader("Tilemap", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Current size: %ux%u", Internal::current_tilemap.width, Internal::current_tilemap.height);
        ImGui::InputText("width", tilemap_w_text, ARRAY_LEN(tilemap_w_text), ImGuiInputTextFlags_CharsDecimal);
        ImGui::InputText("height", tilemap_h_text, ARRAY_LEN(tilemap_h_text), ImGuiInputTextFlags_CharsDecimal);
        if (ImGui::Button("Resize tilemap")) {
            // TODO: Resize tilemap
        }
    }

    if (tile_selected) {
        if (ImGui::CollapsingHeader("Selected Tile", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("%u, %u", selected_tile.x, selected_tile.y);
            ImGui::Text("Type: %u", Ichigo::tile_at(selected_tile));
        }
    }
    ImGui::End();
}

static Util::Optional<Vec2<u32>> tile_at_mouse_cursor() {
    Vec2<i32> viewport_pos = Ichigo::Internal::mouse.pos - vector_cast<i32>(Ichigo::Internal::viewport_origin);
    if (viewport_pos.x < 0 || viewport_pos.y < 0)
        return {};

    Vec2<f32> camera_space = {
        (f32) (SCREEN_TILE_WIDTH  * viewport_pos.x) / Ichigo::Internal::viewport_width,
        (f32) (SCREEN_TILE_HEIGHT * viewport_pos.y) / Ichigo::Internal::viewport_height,
    };

    Vec2<f32> world_space = camera_space + (-1.0f * get_translation2d(Ichigo::Camera::transform));

    if (world_space.x < 0.0f || world_space.y < 0.0f)
        return {};

    return {true, vector_cast<u32>(world_space)};
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

    if (Internal::mouse.left_button.down_this_frame) {
        auto t = tile_at_mouse_cursor();
        if (t.has_value) {
            Vec2<u32> tile = t.value;
            ICHIGO_INFO("Tile clicked: %u,%u", tile.x, tile.y);
            if (tile.x < Internal::current_tilemap.width && tile.y < Internal::current_tilemap.height) {
                tile_selected = true;
                selected_tile = tile;
            } else {
                tile_selected = false;
            }
        } else {
            tile_selected = false;
        }
    }

    if (tile_selected) {
        Ichigo::DrawCommand c;
        c.type = Ichigo::DrawCommandType::SOLID_COLOUR_RECT;
        c.rect = {
            vector_cast<f32>(selected_tile),
            1.0f, 1.0f
        };
        c.colour = {std::fabsf(std::sinf(Internal::platform_get_current_time())), 0.0f, 0.0f, 0.5f};
        Ichigo::push_draw_command(c);
    }
}