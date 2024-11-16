#include "editor.hpp"
#include "ichigo.hpp"
#include "thirdparty/imgui/imgui.h"

enum ActionType {
    FILL,
    RESIZE_TILEMAP,
};

struct FillData {
    Rect<i32> region;
    Ichigo::TileID tile_brush;
};

struct TilemapResizeData {
    u16 new_width;
    u16 new_height;
};

struct EditorAction {
    ActionType type;
    union {
        FillData fill_data;
        TilemapResizeData tilemap_resize_data;
    };
};

struct UndoStack {
    UndoStack(u64 initial_capacity) :
        top(0),
        redo_max_point(0),
        size(0),
        capacity(initial_capacity),
        data((EditorAction *) std::malloc(initial_capacity * sizeof(EditorAction))) {}

    ~UndoStack() { std::free(data); }

    void push(EditorAction value) {
        maybe_resize();
        data[top]      = value;
        redo_max_point = top;
        ++top;
        ++size;
    }

    Util::Optional<EditorAction> redo() {
        if (top <= redo_max_point) {
            ++size;
            return {true, data[top++]};
        }

        return {};
    }

    EditorAction pop() {
        assert(size != 0);
        --size;
        return data[--top];
    }

    u64 top;
    u64 redo_max_point;
    u64 size;
    u64 capacity;
    EditorAction *data;

private:
    void maybe_resize() {
        if (size == capacity) {
            // TODO: Better allocation strategy?
            capacity *= 2;
            data = (EditorAction *) std::realloc(data, capacity);
        }
    }
};

static UndoStack undo_stack(512);
static bool wants_to_save = false;
static Ichigo::TileID tiles_working_copy[ICHIGO_MAX_TILEMAP_SIZE] = {};
static Ichigo::Tilemap saved_tilemap;
static Ichigo::Tilemap tilemap_working_copy = {
    tiles_working_copy,
    0,
    0,
    nullptr,
    0,
    {}
};

static bool tiles_selected                = false;
static Rect<i32> selected_region          = {};
static Ichigo::TileID selected_brush_tile = 0;

static void save_tilemap(const char *path) {
    Ichigo::Internal::PlatformFile *file = Ichigo::Internal::platform_open_file_write(path);

    u16 tilemap_format_version_number = 1;

    Ichigo::Internal::platform_append_file_sync(file, (u8 *) &tilemap_format_version_number, sizeof(tilemap_format_version_number));
    Ichigo::Internal::platform_append_file_sync(file, (u8 *) &tilemap_working_copy.width,    sizeof(tilemap_working_copy.width));
    Ichigo::Internal::platform_append_file_sync(file, (u8 *) &tilemap_working_copy.height,   sizeof(tilemap_working_copy.height));
    Ichigo::Internal::platform_append_file_sync(file, (u8 *) tilemap_working_copy.tiles,     sizeof(Ichigo::TileID) * tilemap_working_copy.width * tilemap_working_copy.height);

    Ichigo::Internal::platform_close_file(file);
}

static void actually_resize_tilemap(u16 new_width, u16 new_height) {
    ICHIGO_INFO("New tilemap size: %ux%u (%u)", new_width, new_height, new_width * new_height);

    if (new_width != tilemap_working_copy.width) {
        if (new_width > tilemap_working_copy.width) {
            u32 width_delta = new_width - tilemap_working_copy.width;

            auto *p = &tilemap_working_copy.tiles[tilemap_working_copy.width];
            for (u32 i = 1; i < tilemap_working_copy.height; ++i) {
                std::memmove(p + width_delta, p, (tilemap_working_copy.height - i) * tilemap_working_copy.width * sizeof(Ichigo::TileID));
                std::memset(p, 0, width_delta * sizeof(Ichigo::TileID));
                p += width_delta + tilemap_working_copy.width;
            }
        } else {
            u32 width_delta = tilemap_working_copy.width - new_width;

            auto *p = &tilemap_working_copy.tiles[(tilemap_working_copy.height - 1) * tilemap_working_copy.width];
            for (u32 i = 1; i < tilemap_working_copy.height; ++i) {
                std::memmove(p - width_delta, p, i * new_width * sizeof(Ichigo::TileID));
                p = &tilemap_working_copy.tiles[(tilemap_working_copy.height - (i + 1)) * tilemap_working_copy.width];
            }
        }
    }

    if (new_height < tilemap_working_copy.height) {
        std::memset(
            &tilemap_working_copy.tiles[new_width * new_height],
            0,
            new_width * tilemap_working_copy.height - new_height * sizeof(Ichigo::TileID)
        );
    }

    tilemap_working_copy.width               = new_width;
    tilemap_working_copy.height              = new_height;
    Ichigo::Internal::current_tilemap.width  = new_width;
    Ichigo::Internal::current_tilemap.height = new_height;
}

static void apply_action(EditorAction action) {
    switch (action.type) {
        case FILL: {
            for (i32 y = action.fill_data.region.pos.y; y < action.fill_data.region.pos.y + action.fill_data.region.h; ++y) {
                for (i32 x = action.fill_data.region.pos.x; x < action.fill_data.region.pos.x + action.fill_data.region.w; ++x) {
                    tilemap_working_copy.tiles[y * tilemap_working_copy.width + x] = action.fill_data.tile_brush;
                }
            }
        } break;

        case RESIZE_TILEMAP: {
            actually_resize_tilemap(action.tilemap_resize_data.new_width, action.tilemap_resize_data.new_height);
        } break;

        default: {
            ICHIGO_ERROR("Undefined editor action in undo stack!");
        }
    }
}

static void rebuild_tilemap() {
    Ichigo::Internal::current_tilemap.width  = saved_tilemap.width;
    Ichigo::Internal::current_tilemap.height = saved_tilemap.height;
    tilemap_working_copy.width               = saved_tilemap.width;
    tilemap_working_copy.height              = saved_tilemap.height;
    tilemap_working_copy.tile_info           = saved_tilemap.tile_info;
    tilemap_working_copy.tile_info_count     = saved_tilemap.tile_info_count;
    std::memcpy(tilemap_working_copy.tiles, saved_tilemap.tiles, saved_tilemap.width * saved_tilemap.height * sizeof(Ichigo::TileID));

    for (u64 i = 0; i < undo_stack.top; ++i) {
        apply_action(undo_stack.data[i]);
    }
}

static void resize_tilemap(u16 new_width, u16 new_height) {
    EditorAction action;
    action.type                           = RESIZE_TILEMAP;
    action.tilemap_resize_data.new_width  = new_width;
    action.tilemap_resize_data.new_height = new_height;

    undo_stack.push(action);
    apply_action(action);
}

static void fill_selected_region(Ichigo::TileID brush) {
    if (!tiles_selected) {
        Ichigo::show_info("Nothing selected.");
    } else {
        EditorAction action;
        action.type                 = FILL;
        action.fill_data.region     = selected_region;
        action.fill_data.tile_brush = brush;

        undo_stack.push(action);
        apply_action(action);
    }
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

            if (new_tilemap_width == 0 || new_tilemap_height == 0 || new_tilemap_width * new_tilemap_height > ICHIGO_MAX_TILEMAP_SIZE) {
                ImGui::OpenPopup("Invalid size");
            } else if (new_tilemap_width < Internal::current_tilemap.width || new_tilemap_height < Internal::current_tilemap.height) {
                ImGui::OpenPopup("Dangerous resize");
            } else {
                resize_tilemap(new_tilemap_width, new_tilemap_height);
            }
        }
    }

    if (wants_to_save) {
        ImGui::OpenPopup("Save as");
    }

    if (ImGui::BeginPopupModal("Save as", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char path_buffer[256];
        ImGui::InputText("Path", path_buffer, ARRAY_LEN(path_buffer));
        if (ImGui::Button("Save")) {
            save_tilemap(path_buffer);
            wants_to_save = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            wants_to_save = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
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

    if (ImGui::CollapsingHeader("Build", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginCombo("Select tile", Internal::current_tilemap.tile_info[selected_brush_tile].name)) {
            for (u32 i = 0; i < Internal::current_tilemap.tile_info_count; ++i) {
                if (i == selected_brush_tile) {
                    ImGui::SetItemDefaultFocus();
                }

                if (ImGui::Selectable(Internal::current_tilemap.tile_info[i].name, i == selected_brush_tile)) {
                    selected_brush_tile = i;
                }
            }

            ImGui::EndCombo();
        }

        ImGui::Text("Brush tile: %u", selected_brush_tile);
        if (ImGui::Button("Fill region (f)")) {
            fill_selected_region(selected_brush_tile);
        }

        if (ImGui::Button("Erase (e)")) {
            fill_selected_region(ICHIGO_AIR_TILE);
        }
    }

    if (tiles_selected) {
        if (selected_region.w == 1 && selected_region.h == 1) {
            if (ImGui::CollapsingHeader("Selected Tile", ImGuiTreeNodeFlags_DefaultOpen)) {
                Vec2<i32> selected_tile   = selected_region.pos;
                TileID tile               = Ichigo::tile_at(vector_cast<u32>(selected_tile));
                const TileInfo &tile_info = Internal::current_tilemap.tile_info[tile];

                ImGui::Text("%u, %u",            selected_tile.x, selected_tile.y);
                ImGui::Text("Name: %s",          tile_info.name);
                ImGui::Text("Friction: %f",      tile_info.friction);
                ImGui::Text("FLAG Tangible: %d", FLAG_IS_SET(tile_info.flags, TileFlag::TANGIBLE));
                ImGui::Text("Cell in sheet: %u", tile_info.cell);
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
    if (Internal::keyboard_state[IK_LEFT_SHIFT].down) {
        camera_speed *= 3.0f;
    }

    if (Internal::keyboard_state[IK_W].down)
        Ichigo::Camera::manual_focus_point.y -= camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_LEFT_CONTROL].up && Internal::keyboard_state[IK_S].down) // FIXME: Stupid hack
        Ichigo::Camera::manual_focus_point.y += camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_A].down)
        Ichigo::Camera::manual_focus_point.x -= camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_D].down)
        Ichigo::Camera::manual_focus_point.x += camera_speed * Ichigo::Internal::dt;

    if (Internal::keyboard_state[IK_F].down_this_frame) {
        fill_selected_region(selected_brush_tile);
    }

    if (Internal::keyboard_state[IK_E].down_this_frame) {
        fill_selected_region(ICHIGO_AIR_TILE);
    }

    if (Internal::keyboard_state[IK_LEFT_CONTROL].down && Internal::keyboard_state[IK_LEFT_SHIFT].down && Internal::keyboard_state[IK_Z].down_this_frame) {
        auto ra = undo_stack.redo();
        if (ra.has_value) {
            apply_action(ra.value);
        }
    } else if (Internal::keyboard_state[IK_LEFT_CONTROL].down && Internal::keyboard_state[IK_Z].down_this_frame && undo_stack.size != 0) {
        undo_stack.pop();
        rebuild_tilemap();
    } else if (Internal::keyboard_state[IK_LEFT_CONTROL].down && Internal::keyboard_state[IK_S].down_this_frame) {
        wants_to_save = true;
    }

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
        Ichigo::DrawCommand c = {};
        c.coordinate_system   = Ichigo::CoordinateSystem::WORLD;
        c.type                = Ichigo::DrawCommandType::SOLID_COLOUR_RECT;

        c.rect = {
            vector_cast<f32>(selected_region.pos),
            (f32) selected_region.w, (f32) selected_region.h
        };

        f64 t = std::sin(Internal::platform_get_current_time() * 3);
        t *= t;
        t = 0.5f + 0.5f * t;

        c.colour = {ichigo_lerp(0.3f, t, 0.8f), 0.0f, 0.0f, 0.5f};
        Ichigo::push_draw_command(c);
    }
}

// TODO: We can use this to clear all changes as well. It is just used to restore the original current_tilemap tiles pointer.
void Ichigo::Editor::before_open() {
    std::memcpy(&saved_tilemap, &Internal::current_tilemap, sizeof(Tilemap));

    tilemap_working_copy.width           = Internal::current_tilemap.width;
    tilemap_working_copy.height          = Internal::current_tilemap.height;
    tilemap_working_copy.tile_info       = Internal::current_tilemap.tile_info;
    tilemap_working_copy.tile_info_count = Internal::current_tilemap.tile_info_count;
    std::memcpy(tilemap_working_copy.tiles, Internal::current_tilemap.tiles, Internal::current_tilemap.width * Internal::current_tilemap.height * sizeof(TileID));

    // TODO: This is kind of a hack. We do this so that the renderer shows the modified tilemap while we are editing it.
    Internal::current_tilemap.tiles = tilemap_working_copy.tiles;
}

void Ichigo::Editor::before_close() {
    Internal::current_tilemap.width           = tilemap_working_copy.width;
    Internal::current_tilemap.height          = tilemap_working_copy.height;
    Internal::current_tilemap.tile_info       = tilemap_working_copy.tile_info;
    Internal::current_tilemap.tile_info_count = tilemap_working_copy.tile_info_count;

    Internal::current_tilemap.tiles = saved_tilemap.tiles;
    std::memcpy(Internal::current_tilemap.tiles, tilemap_working_copy.tiles, tilemap_working_copy.width * tilemap_working_copy.height * sizeof(TileID));
}
