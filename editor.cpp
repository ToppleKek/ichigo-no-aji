#include "editor.hpp"
#include "ichigo.hpp"
#include "thirdparty/imgui/imgui.h"

static bool tiles_selected       = false;
static Rect<i32> selected_region = {};

static void resize_tilemap(u16 new_width, u16 new_height) {
    ICHIGO_INFO("New tilemap size: %ux%u (%u)", new_width, new_height, new_width * new_height);

    if (new_width != Ichigo::Internal::current_tilemap.width) {
        if (new_width > Ichigo::Internal::current_tilemap.width) {
            u32 width_delta = new_width - Ichigo::Internal::current_tilemap.width;

            auto *p = &Ichigo::Internal::current_tilemap.tiles[Ichigo::Internal::current_tilemap.width];
            for (u32 i = 1; i < Ichigo::Internal::current_tilemap.height; ++i) {
                std::memmove(p + width_delta, p, (Ichigo::Internal::current_tilemap.height - i) * Ichigo::Internal::current_tilemap.width * sizeof(u16));
                std::memset(p, 0, width_delta * sizeof(u16));
                p += width_delta + Ichigo::Internal::current_tilemap.width;
            }
        } else {
            u32 width_delta = Ichigo::Internal::current_tilemap.width - new_width;

            auto *p = &Ichigo::Internal::current_tilemap.tiles[(Ichigo::Internal::current_tilemap.height - 1) * Ichigo::Internal::current_tilemap.width];
            for (u32 i = 1; i < Ichigo::Internal::current_tilemap.height; ++i) {
                std::memmove(p - width_delta, p, i * new_width * sizeof(u16));
                p = &Ichigo::Internal::current_tilemap.tiles[(Ichigo::Internal::current_tilemap.height - (i + 1)) * Ichigo::Internal::current_tilemap.width];
            }
        }
    }

    if (new_height < Ichigo::Internal::current_tilemap.height) {
        std::memset(&Ichigo::Internal::current_tilemap.tiles[new_width * new_height], 0, new_width * Ichigo::Internal::current_tilemap.height - new_height * sizeof(u16));
    }

    Ichigo::Internal::current_tilemap.width  = new_width;
    Ichigo::Internal::current_tilemap.height = new_height;
}

void Ichigo::Editor::render_ui() {
    static char tilemap_w_text[16];
    static char tilemap_h_text[16];
    static u16 new_tilemap_width  = 0;
    static u16 new_tilemap_height = 0;
    ImGui::SetNextWindowPos({Ichigo::Internal::window_width * 0.8f, 0.0f});
    ImGui::SetNextWindowSize({Ichigo::Internal::window_width * 0.2f, (f32) Ichigo::Internal::window_height});
    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    if (ImGui::CollapsingHeader("Tilemap", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Current size: %ux%u", Internal::current_tilemap.width, Internal::current_tilemap.height);
        ImGui::InputText("width", tilemap_w_text, ARRAY_LEN(tilemap_w_text), ImGuiInputTextFlags_CharsDecimal);
        ImGui::InputText("height", tilemap_h_text, ARRAY_LEN(tilemap_h_text), ImGuiInputTextFlags_CharsDecimal);
        if (ImGui::Button("Resize tilemap")) {
            new_tilemap_width  = std::atoi(tilemap_w_text);
            new_tilemap_height = std::atoi(tilemap_h_text);

            if (new_tilemap_width == 0 || new_tilemap_height == 0 || new_tilemap_width * new_tilemap_height > ICHIGO_MAX_TILEMAP_SIZE)
                ImGui::OpenPopup("Invalid size");
            else if (new_tilemap_width < Internal::current_tilemap.width || new_tilemap_height < Internal::current_tilemap.height)
                ImGui::OpenPopup("Dangerous resize");
            else
                resize_tilemap(new_tilemap_width, new_tilemap_height);
        }
    }

    if (ImGui::BeginPopupModal("Invalid size", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Cannot resize tilemap to %ux%u as the total tilemap area (%u) is too large or invalid.", new_tilemap_width, new_tilemap_height, new_tilemap_width * new_tilemap_height);
        if (ImGui::Button("OK"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Dangerous resize", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(
            "Resizing the tilemap to %ux%u is smaller in at least one dimention (current size is %ux%u). This will destroy data!",
            new_tilemap_width,
            new_tilemap_height,
            Internal::current_tilemap.width,
            Internal::current_tilemap.height
        );

        if (ImGui::Button("OK")) {
            resize_tilemap(new_tilemap_width, new_tilemap_height);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if (tiles_selected) {
        if (selected_region.w == 1 && selected_region.h == 1) {
            if (ImGui::CollapsingHeader("Selected Tile", ImGuiTreeNodeFlags_DefaultOpen)) {
                Vec2<i32> selected_tile   = selected_region.pos;
                u16 tile                  = Ichigo::tile_at(vector_cast<u32>(selected_tile));
                const TileInfo &tile_info = Internal::current_tilemap.tile_info[tile];

                ImGui::Text("%u, %u",            selected_tile.x, selected_tile.y);
                ImGui::Text("Name: %s",          tile_info.name);
                ImGui::Text("Friction: %f",      tile_info.friction);
                ImGui::Text("FLAG Tangible: %d", FLAG_IS_SET(tile_info.flags, TileFlag::TANGIBLE));
                ImGui::Text("Texture ID: %u",    tile_info.texture_id);
                ImGui::Text("Tile ID: %u",       tile);
            }
        } else {
            if (ImGui::CollapsingHeader("Selected Tiles", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("%ux%u region selected at %u, %u", abs(selected_region.w), abs(selected_region.h), selected_region.pos.x, selected_region.pos.y);
            }
        }
    }
    ImGui::End();
}

static Util::Optional<Vec2<f32>> screen_space_to_camera_space(Vec2<i32> screen_space_coord) {
    Vec2<i32> viewport_pos = screen_space_coord - vector_cast<i32>(Ichigo::Internal::viewport_origin);
    if (viewport_pos.x < 0 || viewport_pos.y < 0) {
        return {};
    }

    Vec2<f32> camera_space = {
        (f32) (SCREEN_TILE_WIDTH  * viewport_pos.x) / Ichigo::Internal::viewport_width,
        (f32) (SCREEN_TILE_HEIGHT * viewport_pos.y) / Ichigo::Internal::viewport_height,
    };

    return {true, camera_space};
}

static Util::Optional<Vec2<f32>> screen_space_to_world_space(Vec2<i32> screen_space_coord) {
    auto cs = screen_space_to_camera_space(screen_space_coord);
    if (!cs.has_value) {
        return {};
    }

    Vec2<f32> camera_space = cs.value;
    Vec2<f32> world_space  = camera_space + (-1.0f * get_translation2d(Ichigo::Camera::transform));

    if (world_space.x < 0.0f || world_space.y < 0.0f) {
        return {};
    }

    return {true, world_space};
}

static Util::Optional<Vec2<u32>> tile_at_mouse_coordinate(Vec2<i32> mouse_coord) {
    auto ws = screen_space_to_world_space(mouse_coord);

    if (ws.has_value) {
        Vec2<f32> world_space = ws.value;

        // The tile coord is just the world space coord of the mouse truncated towards 0
        return {true, vector_cast<u32>(world_space)};
    }

    return {};
}

#define BASE_CAMERA_SPEED 10.0f
void Ichigo::Editor::update() {
    f32 camera_speed = BASE_CAMERA_SPEED;
    if (Internal::keyboard_state[IK_LEFT_SHIFT].down)
        camera_speed *= 3.0f;

    if (Internal::keyboard_state[IK_W].down)
        Ichigo::Camera::manual_focus_point.y -= camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_S].down)
        Ichigo::Camera::manual_focus_point.y += camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_A].down)
        Ichigo::Camera::manual_focus_point.x -= camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_D].down)
        Ichigo::Camera::manual_focus_point.x += camera_speed * Ichigo::Internal::dt;

    static Vec2<f32> pan_start_pos;
    static Vec2<f32> saved_focus_point;
    if (Internal::mouse.middle_button.down_this_frame) {
        auto cs = screen_space_to_camera_space(Internal::mouse.pos);
        if (cs.has_value) {
            pan_start_pos     = cs.value;
            saved_focus_point = Camera::manual_focus_point;
        }
    }

    if (Internal::mouse.middle_button.down) {
        auto cs = screen_space_to_camera_space(Internal::mouse.pos);
        if (cs.has_value) {
            Vec2<f32> mouse_camera_space = cs.value;
            Camera::manual_focus_point   = saved_focus_point - (mouse_camera_space - pan_start_pos);
        }
    }

    static Vec2<u32> mouse_down_tile;
    if (Internal::mouse.left_button.down_this_frame) {
        auto t = tile_at_mouse_coordinate(Internal::mouse.pos);
        if (t.has_value) {
            Vec2<u32> tile_coord = t.value;
            mouse_down_tile = tile_coord;
            ICHIGO_INFO("Tile clicked: %u,%u", tile_coord.x, tile_coord.y);
            if (tile_coord.x < Internal::current_tilemap.width && tile_coord.y < Internal::current_tilemap.height) {
                tiles_selected  = true;
                selected_region = {vector_cast<i32>(tile_coord), 1, 1};
            } else {
                tiles_selected = false;
            }
        } else {
            tiles_selected = false;
        }
    } else if (Internal::mouse.left_button.down) {
        auto t = tile_at_mouse_coordinate(Internal::mouse.pos);
        if (t.has_value) {
            selected_region.pos.x = MIN(t.value.x, mouse_down_tile.x);
            selected_region.pos.y = MIN(t.value.y, mouse_down_tile.y);
            u32 w                 = DISTANCE(t.value.x, mouse_down_tile.x);
            u32 h                 = DISTANCE(t.value.y, mouse_down_tile.y);
            selected_region.w     = w == 0 ? 1 : w + 1;
            selected_region.h     = h == 0 ? 1 : h + 1;
        }
    }

    if (tiles_selected) {
        Ichigo::DrawCommand c;
        c.type = Ichigo::DrawCommandType::SOLID_COLOUR_RECT;
        c.rect = {
            vector_cast<f32>(selected_region.pos),
            (f32) selected_region.w, (f32) selected_region.h
        };

        f64 t = std::sin(Internal::platform_get_current_time() * 3);
        t *= t;
        t = 0.5f + 0.5f * t;

        c.colour = {lerp(0.3f, t, 0.8f), 0.0f, 0.0f, 0.5f};
        Ichigo::push_draw_command(c);
    }
}
