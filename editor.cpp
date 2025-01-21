/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Tilemap and tileset editor.

    Author:      Braeden Hong
    Last edited: 2024/11/25
*/


#include "editor.hpp"
#include "ichigo.hpp"
#include "thirdparty/imgui/imgui.h"

EMBED("assets/editor-move-arrow.png", move_arrow_png)

#define MAX_CAMERA_ZOOM 4.0f
#define MIN_CAMERA_ZOOM 0.3f

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

// TODO: @heap
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

    Bana::Optional<EditorAction> redo() {
        if (top <= redo_max_point) {
            ++size;
            return {data[top++]};
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

static Ichigo::TextureID move_arrow_texture = 0;

static UndoStack undo_stack(512);
static bool wants_to_save = false;
static bool wants_to_reset = false;
static bool tileset_editor_is_open = false;
static Ichigo::TileID tiles_working_copy[ICHIGO_MAX_TILEMAP_SIZE] = {};
static Ichigo::TileInfo tile_info_working_copy[ICHIGO_MAX_UNIQUE_TILES] = {};
static Ichigo::Tilemap saved_tilemap;
static Vec2<f32> saved_screen_tile_dimensions = {};
static f32 zoom_scale = 1.0f;

// The working copy of the tilemap. This copy is edited by the editor, and then copied back to the engine when you exit the editor (in before_close()).
static Ichigo::Tilemap tilemap_working_copy = {
    tiles_working_copy,
    0,
    0,
    tile_info_working_copy,
    0,
    {}
};

static bool tiles_selected                = false;
static Rect<i32> selected_region          = {};
static i32 selected_entity_descriptor_idx = -1;
// The "brush_tile" is the current tile that you are "painting" with. This is the tile type that will be filled when using the fill tool.
static Ichigo::TileID selected_brush_tile = 0;

// Save the tilemap to a ichigotm file. The file format is as follows:
// u16 version number
// u32 number of unique tiles, n
// n TileInfo structures
// u32 tilemap width
// u32 tilemap height
// width * height TileIDs
static void save_tilemap(Bana::String path) {
    MAKE_STACK_STRING(tilemap_path, path.length + sizeof(".ichigotm"));
    MAKE_STACK_STRING(entity_descriptor_path, path.length + sizeof(".ichigoed"));

    string_concat(tilemap_path, path);
    string_concat(tilemap_path, ".ichigotm");

    string_concat(entity_descriptor_path, path);
    string_concat(entity_descriptor_path, ".ichigoed");

    Ichigo::Internal::PlatformFile *tilemap_file = Ichigo::Internal::platform_open_file_write(tilemap_path);
    Ichigo::Internal::PlatformFile *entity_file  = Ichigo::Internal::platform_open_file_write(entity_descriptor_path);

    assert(tilemap_file);
    assert(entity_file);

    u16 tilemap_format_version_number = 2;

    Ichigo::Internal::platform_append_file_sync(tilemap_file, (u8 *) &tilemap_format_version_number,        sizeof(tilemap_format_version_number));

    Ichigo::Internal::platform_append_file_sync(tilemap_file, (u8 *) &tilemap_working_copy.tile_info_count, sizeof(tilemap_working_copy.tile_info_count));
    Ichigo::Internal::platform_append_file_sync(tilemap_file, (u8 *) tilemap_working_copy.tile_info,        sizeof(Ichigo::TileInfo) * tilemap_working_copy.tile_info_count);

    Ichigo::Internal::platform_append_file_sync(tilemap_file, (u8 *) &tilemap_working_copy.width,           sizeof(tilemap_working_copy.width));
    Ichigo::Internal::platform_append_file_sync(tilemap_file, (u8 *) &tilemap_working_copy.height,          sizeof(tilemap_working_copy.height));
    Ichigo::Internal::platform_append_file_sync(tilemap_file, (u8 *) tilemap_working_copy.tiles,            sizeof(Ichigo::TileID) * tilemap_working_copy.width * tilemap_working_copy.height);

    Ichigo::Internal::platform_close_file(tilemap_file);

    const char header[] = "static Ichigo::EntityDescriptor entity_descriptors[] = {\n";
    // NOTE: sizeof header MINUS ONE because we don't want to write the null terminator
    Ichigo::Internal::platform_append_file_sync(entity_file, (u8 *) header, sizeof(header) - 1);

    auto *current_descriptors = Ichigo::Game::level_entity_descriptors();

    Bana::String line = Bana::make_string(1024, Ichigo::Internal::temp_allocator);
    for (u32 i = 0; i < current_descriptors->size; ++i) {
        auto descriptor = (*current_descriptors)[i];
        line.length = 0;
        Bana::string_format(line, "    { \"%s\", %s, {%ff, %ff}, %lld },\n", descriptor.name, descriptor.name, descriptor.pos.x, descriptor.pos.y, descriptor.data);
        Ichigo::Internal::platform_append_file_sync(entity_file, (u8 *) line.data, line.length);
    }

    Ichigo::Internal::platform_append_file_sync(entity_file, (const u8 *) "};", 2);
    Ichigo::Internal::platform_close_file(entity_file);
}

// Commit a tilemap resize. Called when the tilemap actually needs to be resized. Eg. the undo stack is rebuilding the tilemap.
static void actually_resize_tilemap(u16 new_width, u16 new_height) {
    ICHIGO_INFO("New tilemap size: %ux%u (%u)", new_width, new_height, new_width * new_height);

    // Change the width of the tilemap by padding the end of the rows with 0s.
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

    // NOTE: If the new height is larger, nothing needs to be done since we make sure to zero out all height reductions here.
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

// Apply some editor action to the tilemap. Called when performing an action, and when the undo stack rebuilds the tilemap.
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

// Rebuild the tilemap. The tilemap is rebuilt when you undo; minus the action on the top of the undo stack.
static void rebuild_tilemap() {
    Ichigo::Internal::current_tilemap.width  = saved_tilemap.width;
    Ichigo::Internal::current_tilemap.height = saved_tilemap.height;
    tilemap_working_copy.width               = saved_tilemap.width;
    tilemap_working_copy.height              = saved_tilemap.height;
    // TODO: Undo/redo in the tileset editor
    // tilemap_working_copy.tile_info           = saved_tilemap.tile_info;
    // tilemap_working_copy.tile_info_count     = saved_tilemap.tile_info_count;
    std::memcpy(tilemap_working_copy.tiles, saved_tilemap.tiles, saved_tilemap.width * saved_tilemap.height * sizeof(Ichigo::TileID));

    for (u64 i = 0; i < undo_stack.top; ++i) {
        apply_action(undo_stack.data[i]);
    }
}

static void reset_tilemap() {
    std::memset(tilemap_working_copy.tiles, 0, ICHIGO_MAX_TILEMAP_SIZE * sizeof(Ichigo::TileID));

    Ichigo::Internal::current_tilemap.width  = 1;
    Ichigo::Internal::current_tilemap.height = 1;
    tilemap_working_copy.width               = 1;
    tilemap_working_copy.height              = 1;
}

// Push a tilemap resize action onto the undo stack and apply the action.
static void resize_tilemap(u16 new_width, u16 new_height) {
    EditorAction action;
    action.type                           = RESIZE_TILEMAP;
    action.tilemap_resize_data.new_width  = new_width;
    action.tilemap_resize_data.new_height = new_height;

    undo_stack.push(action);
    apply_action(action);
}

// Push a fill region action onto the undo stack and apply the action.
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

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Zoom", &zoom_scale, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM, "%2fx");
        if (ImGui::Button("Reset zoom")) {
            zoom_scale = 1.0f;
        }
    }

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

        if (ImGui::Button("Open tileset editor")) {
            tileset_editor_is_open = true;
        }
    }

    if (wants_to_save) {
        ImGui::OpenPopup("Save as");
    }

    if (wants_to_reset && !wants_to_save) {
        ImGui::OpenPopup("Reset tilemap");
    }

    if (ImGui::BeginPopupModal("Save as", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char path_buffer[256];
        ImGui::InputText("Path", path_buffer, ARRAY_LEN(path_buffer));
        if (ImGui::Button("Save")) {
            save_tilemap(Bana::temp_string(path_buffer));
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

    if (ImGui::BeginPopupModal("Reset tilemap", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Really reset the tilemap?");
        if (ImGui::Button("Reset")) {
            reset_tilemap();
            wants_to_reset = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            wants_to_reset = false;
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
            ImGuiListClipper clipper;
            clipper.Begin(Internal::current_tilemap.tile_info_count);

            while (clipper.Step()) {
                for (i32 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    if (i == selected_brush_tile) {
                        ImGui::SetItemDefaultFocus();
                    }

                    if (ImGui::Selectable(Internal::current_tilemap.tile_info[i].name, i == selected_brush_tile)) {
                        selected_brush_tile = i;
                    }
                }

            }

            ImGui::EndCombo();
        }

        ImGui::Text("Selected at: %d,%d", selected_region.pos.x, selected_region.pos.y);
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
                Vec2<i32> selected_tile     = selected_region.pos;
                TileID tile                 = Ichigo::tile_at(selected_tile);
                TileInfo &current_tile_info = Internal::current_tilemap.tile_info[tile];

                i32 i32_one = 1;
                f32 f32_one = 1.0f;

                // FLAGS
                bool tangible = FLAG_IS_SET(current_tile_info.flags, TileFlag::TANGIBLE);

                ImGui::InputText("Name", current_tile_info.name, ARRAY_LEN(current_tile_info.name));
                ImGui::InputScalar("Friction", ImGuiDataType_Float, &current_tile_info.friction, &f32_one, nullptr, "%f");
                ImGui::Checkbox("FLAG Tangible", &tangible);
                ImGui::InputScalar("Cell in sheet", ImGuiDataType_S32, &current_tile_info.cell, &i32_one, nullptr, "%d");
                ImGui::Text("Tile ID: %u", tile);

                if (!tangible) CLEAR_FLAG(current_tile_info.flags, TileFlag::TANGIBLE);
                else           SET_FLAG(current_tile_info.flags, TileFlag::TANGIBLE);

            }
        } else {
            if (ImGui::CollapsingHeader("Selected Tiles", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("%ux%u region selected at %u, %u", abs(selected_region.w), abs(selected_region.h), selected_region.pos.x, selected_region.pos.y);
            }
        }
    }

    if (selected_entity_descriptor_idx != -1) {
        if (ImGui::CollapsingHeader("Selected Entity Descriptor", ImGuiTreeNodeFlags_DefaultOpen)) {
            // ImGui::InputScalar
        }
    }

    ImGui::End();

    if (tileset_editor_is_open) {
        static TileID tile_being_edited = 0;

        ImGui::Begin("Tileset", &tileset_editor_is_open, ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Tiles")) {
                if (ImGui::MenuItem("New tile")) {
                    tile_being_edited = tilemap_working_copy.tile_info_count;
                    ++tilemap_working_copy.tile_info_count;
                    // FIXME: Hack. See other notes about this.
                    Ichigo::Internal::current_tilemap.tile_info_count = tilemap_working_copy.tile_info_count;

                    std::strcpy(tilemap_working_copy.tile_info[tile_being_edited].name, "new");
                }

                // TODO
                if (ImGui::MenuItem("Remap tile")) {

                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::BeginChild("##left", ImVec2(200, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
        if (ImGui::BeginListBox("##select", ImVec2(-FLT_MIN, -FLT_MIN))) {
            ImGuiListClipper clipper;
            clipper.Begin(tilemap_working_copy.tile_info_count);

            while (clipper.Step()) {
                for (i32 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    if (i == tile_being_edited) {
                        ImGui::SetItemDefaultFocus();
                    }

                    if (ImGui::Selectable(tilemap_working_copy.tile_info[i].name, i == tile_being_edited)) {
                        tile_being_edited = i;
                    }
                }
            }

            ImGui::EndListBox();
        }

        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("##right", ImVec2(0, 0));
        TileInfo &current_tile_info = tilemap_working_copy.tile_info[tile_being_edited];

        i32 i32_one = 1;
        f32 f32_one = 1.0f;

        // FLAGS
        bool tangible = FLAG_IS_SET(current_tile_info.flags, TileFlag::TANGIBLE);

        ImGui::InputText("Name",            current_tile_info.name, ARRAY_LEN(current_tile_info.name));
        ImGui::InputScalar("Friction",      ImGuiDataType_Float, &current_tile_info.friction, &f32_one, nullptr, "%f");
        ImGui::Checkbox("FLAG Tangible",    &tangible);
        ImGui::InputScalar("Cell in sheet", ImGuiDataType_S32, &current_tile_info.cell, &i32_one, nullptr, "%d");
        ImGui::Text("Tile ID: %u",          tile_being_edited);

        if (!tangible) CLEAR_FLAG(current_tile_info.flags, TileFlag::TANGIBLE);
        else           SET_FLAG(current_tile_info.flags, TileFlag::TANGIBLE);

        ImGui::EndChild();

        ImGui::End();
    }
}

void Ichigo::Editor::render() {
    // Draw entity descriptors
    Ichigo::TextStyle style;
    style.scale     = 0.5f;
    style.alignment = Ichigo::TextAlignment::LEFT;
    style.colour    = {1.0f, 0.0f, 1.0f, 0.8f};

    f64 t = std::sin(Internal::platform_get_current_time() * 3);
    t *= t;
    t = 0.5f + 0.5f * t;

    auto *current_descriptors = Ichigo::Game::level_entity_descriptors();
    for (u32 i = 0; i < current_descriptors->size; ++i) {
        auto descriptor = (*current_descriptors)[i];
        char text[sizeof(descriptor.name) + 1] = {};
        std::snprintf(text, sizeof(text), "%s", descriptor.name);

        u32 text_length = std::strlen(text);
        f32 text_width = get_text_width(text, text_length, style);

        Ichigo::world_render_solid_colour_rect(
            {
                descriptor.pos + Vec2<f32>{0.0f, -0.20f},
                text_width, 0.2f
            },
            {0.0f, 0.0f, 0.0f, 0.5f}
        );

        Ichigo::render_text(descriptor.pos + Vec2<f32>{0.0f, -0.05f}, text, text_length, Ichigo::CoordinateSystem::WORLD, style);

        Ichigo::world_render_solid_colour_rect(
            {
                descriptor.pos,
                1.0f, 1.0f
            },
            (i32) i == selected_entity_descriptor_idx ? Vec4<f32>{ichigo_lerp(0.3f, t, 0.8f), 0.0f, ichigo_lerp(0.3f, t, 0.8f), 0.5f} : Vec4<f32>{0.5f, 0.0f, 0.5f, 0.5f}
        );
    }
}

// NOTE: Actual "screen space". Eg. the mouse cursor position.
static Bana::Optional<Vec2<f32>> screen_space_to_camera_space(Vec2<i32> screen_space_coord) {
    Vec2<i32> viewport_pos = screen_space_coord - vector_cast<i32>(Ichigo::Internal::viewport_origin);
    if (viewport_pos.x < 0 || viewport_pos.y < 0) {
        return {};
    }

    Vec2<f32> camera_space = {
        (f32) (Ichigo::Camera::screen_tile_dimensions.x * viewport_pos.x) / Ichigo::Internal::viewport_width,
        (f32) (Ichigo::Camera::screen_tile_dimensions.y * viewport_pos.y) / Ichigo::Internal::viewport_height,
    };

    return {camera_space};
}

// NOTE: Actual "screen space". Eg. the mouse cursor position.
static Bana::Optional<Vec2<f32>> screen_space_to_world_space(Vec2<i32> screen_space_coord) {
    auto cs = screen_space_to_camera_space(screen_space_coord);
    if (!cs.has_value) {
        return {};
    }

    Vec2<f32> camera_space = cs.value;
    Vec2<f32> world_space  = camera_space + (-1.0f * get_translation2d(Ichigo::Camera::transform));

    if (world_space.x < 0.0f || world_space.y < 0.0f) {
        return {};
    }

    return {world_space};
}

static Bana::Optional<Vec2<i32>> tile_at_mouse_coordinate(Vec2<i32> mouse_coord) {
    auto ws = screen_space_to_world_space(mouse_coord);

    if (ws.has_value) {
        Vec2<f32> world_space = ws.value;

        // The tile coord is just the world space coord of the mouse truncated towards 0
        return {vector_cast<i32>(world_space)};
    }

    return {};
}

#define BASE_CAMERA_SPEED 10.0f
void Ichigo::Editor::update() {
    zoom_scale += Ichigo::Internal::mouse.scroll_wheel_delta_this_frame * 0.05f;
    zoom_scale = clamp(zoom_scale, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);
    Ichigo::Camera::screen_tile_dimensions = saved_screen_tile_dimensions * 1.0f / zoom_scale;


    f32 camera_speed = BASE_CAMERA_SPEED;

    // Triple the camera speed if shift is held.
    if (Internal::keyboard_state[IK_LEFT_SHIFT].down) {
        camera_speed *= 3.0f;
    }

    // Move the camera with WSAD
    if (Internal::keyboard_state[IK_W].down)
        Ichigo::Camera::manual_focus_point.y -= camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_LEFT_CONTROL].up && Internal::keyboard_state[IK_S].down) // FIXME: Stupid hack
        Ichigo::Camera::manual_focus_point.y += camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_A].down)
        Ichigo::Camera::manual_focus_point.x -= camera_speed * Ichigo::Internal::dt;
    if (Internal::keyboard_state[IK_D].down)
        Ichigo::Camera::manual_focus_point.x += camera_speed * Ichigo::Internal::dt;

    // Keyboard shortcuts
    if (Internal::keyboard_state[IK_F].down_this_frame) {
        fill_selected_region(selected_brush_tile);
    }

    if (Internal::keyboard_state[IK_E].down_this_frame) {
        fill_selected_region(ICHIGO_AIR_TILE);
    }

    // Pick-tile ("eyedropper") tool. Select the tile you right click as the brush tile.
    if (Internal::mouse.right_button.down_this_frame) {
        auto t = tile_at_mouse_coordinate(Internal::mouse.pos);
        if (t.has_value) {
            TileID tile_id       = tile_at(t.value);
            const TileInfo &info = Internal::current_tilemap.tile_info[tile_id];
            selected_brush_tile  = tile_id;
            char str[64]         = {};

            std::snprintf(str, ARRAY_LEN(str), "Selected: %s", info.name);
            show_info(str, std::strlen(str));
        }
    }

    // Undo/redo keybinds.
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
    } else if (Internal::keyboard_state[IK_LEFT_CONTROL].down && Internal::keyboard_state[IK_N].down_this_frame) {
        wants_to_reset = true;
    }

    // Mouse panning. Drag with the middle mouse button to pan the camera.
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

    // Tile selection.
    static Vec2<i32> mouse_down_tile;
    static bool moving_entity = false;

    auto *current_descriptors = Ichigo::Game::level_entity_descriptors();

    if (Internal::keyboard_state[IK_M].down_this_frame && selected_entity_descriptor_idx != -1) {
        moving_entity = !moving_entity;
        show_info("Toggled entity move");
    }

    if (Internal::mouse.left_button.down_this_frame) {
        auto ws = screen_space_to_world_space(Internal::mouse.pos);

        if (ws.has_value) {
            for (u32 i = 0; i < current_descriptors->size; ++i) {
                if (rectangle_contains_point({(*current_descriptors)[i].pos, 1.0f, 1.0f}, ws.value)) {
                    if (moving_entity && selected_entity_descriptor_idx == (i32) i) goto found;

                    selected_entity_descriptor_idx = (i32) i;
                    tiles_selected                 = false;
                    moving_entity                  = false;

                    Ichigo::show_info("Selected ENT");
                    goto found;
                }
            }

            selected_entity_descriptor_idx = -1;
found:;
        }

        if (selected_entity_descriptor_idx == -1) {
            auto t = tile_at_mouse_coordinate(Internal::mouse.pos);
            if (t.has_value) {
                Vec2<i32> tile_coord = t.value;
                mouse_down_tile = tile_coord;
                if (tile_coord.x < (i32) Internal::current_tilemap.width && tile_coord.y < (i32) Internal::current_tilemap.height) {
                    tiles_selected  = true;
                    selected_region = {vector_cast<i32>(tile_coord), 1, 1};
                } else {
                    tiles_selected = false;
                }
            } else {
                tiles_selected = false;
            }
        }
    } else if (Internal::mouse.left_button.down) {
        if (selected_entity_descriptor_idx != -1 && moving_entity) {
            auto ws = screen_space_to_world_space(Internal::mouse.pos);

            if (ws.has_value) {
                auto *current_descriptors = Ichigo::Game::level_entity_descriptors();
                (*current_descriptors)[selected_entity_descriptor_idx].pos = ws.value;
            }
        }
        // The left button is still down, so select a region.
        auto t = tile_at_mouse_coordinate(Internal::mouse.pos);
        if (t.has_value) {
            // NOTE: The "pos" of the region rectangle is always at the top left.
            selected_region.pos.x = MIN(t.value.x, mouse_down_tile.x);
            selected_region.pos.y = MIN(t.value.y, mouse_down_tile.y);
            u32 w                 = DISTANCE(t.value.x, mouse_down_tile.x);
            u32 h                 = DISTANCE(t.value.y, mouse_down_tile.y);
            selected_region.w     = w == 0 ? 1 : w + 1;
            selected_region.h     = h == 0 ? 1 : h + 1;
        }
    }

    if (moving_entity && selected_entity_descriptor_idx != -1) {
        Vec2<f32> entity_pos = (*current_descriptors)[selected_entity_descriptor_idx].pos;
        Vec4<f32> tint = {1.0f, 1.0f, 1.0f, 0.5f};
        Ichigo::render_rect_deferred(Ichigo::CoordinateSystem::WORLD, {entity_pos + Vec2<f32>{1.0f, 0.0f}, 1.0f, 1.0f}, move_arrow_texture, m4identity(),       tint);
        Ichigo::render_rect_deferred(Ichigo::CoordinateSystem::WORLD, {entity_pos + Vec2<f32>{1.0f, 1.0f}, 1.0f, 1.0f}, move_arrow_texture, rotation2d(90.0f),  tint);
        Ichigo::render_rect_deferred(Ichigo::CoordinateSystem::WORLD, {entity_pos + Vec2<f32>{0.0f, 1.0f}, 1.0f, 1.0f}, move_arrow_texture, rotation2d(180.0f), tint);
        Ichigo::render_rect_deferred(Ichigo::CoordinateSystem::WORLD, {entity_pos                        , 1.0f, 1.0f}, move_arrow_texture, rotation2d(270.0f), tint);
    }

    // Draw pulsing red selection region rectangle.
    if (tiles_selected) {
        Ichigo::DrawCommand c = {};
        c.coordinate_system   = Ichigo::CoordinateSystem::WORLD;
        c.type                = Ichigo::DrawCommandType::SOLID_COLOUR_RECT;
        c.transform           = m4identity_f32;

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

// Run before the tilemap editor opens. Copy the current tilemap information to our working copy.
void Ichigo::Editor::before_open() {
    if (move_arrow_texture == 0) move_arrow_texture = Ichigo::load_texture(move_arrow_png, move_arrow_png_len);

    saved_screen_tile_dimensions = Ichigo::Camera::screen_tile_dimensions;

    // TODO: We can use this to clear all changes as well. It is just used to restore the original current_tilemap tiles pointer.
    std::memcpy(&saved_tilemap, &Internal::current_tilemap, sizeof(Tilemap));

    tilemap_working_copy.width           = Internal::current_tilemap.width;
    tilemap_working_copy.height          = Internal::current_tilemap.height;
    tilemap_working_copy.tile_info_count = Internal::current_tilemap.tile_info_count;

    std::memcpy(tilemap_working_copy.tile_info, Internal::current_tilemap.tile_info, Internal::current_tilemap.tile_info_count * sizeof(TileInfo));
    std::memcpy(tilemap_working_copy.tiles, Internal::current_tilemap.tiles, Internal::current_tilemap.width * Internal::current_tilemap.height * sizeof(TileID));

    // TODO: This is kind of a hack. We do this so that the renderer shows the modified tilemap while we are editing it.
    Internal::current_tilemap.tiles     = tilemap_working_copy.tiles;
    Internal::current_tilemap.tile_info = tilemap_working_copy.tile_info;
}

// Run before the tilemap editor closes. Copy the working copy data to the current tilemap.
void Ichigo::Editor::before_close() {
    Ichigo::Camera::screen_tile_dimensions    = saved_screen_tile_dimensions;

    Internal::current_tilemap.width           = tilemap_working_copy.width;
    Internal::current_tilemap.height          = tilemap_working_copy.height;
    Internal::current_tilemap.tile_info       = tilemap_working_copy.tile_info;
    Internal::current_tilemap.tile_info_count = tilemap_working_copy.tile_info_count;

    Internal::current_tilemap.tiles     = saved_tilemap.tiles;
    Internal::current_tilemap.tile_info = saved_tilemap.tile_info;
    std::memcpy(Internal::current_tilemap.tile_info, tilemap_working_copy.tile_info, tilemap_working_copy.tile_info_count * sizeof(TileInfo));
    std::memcpy(Internal::current_tilemap.tiles, tilemap_working_copy.tiles, tilemap_working_copy.width * tilemap_working_copy.height * sizeof(TileID));
}
