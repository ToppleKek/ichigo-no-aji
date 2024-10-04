#include <cassert>

#include "camera.hpp"
#include "common.hpp"
#include "entity.hpp"
#include "math.hpp"
#include "ichigo.hpp"

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

EMBED("noto.ttf", noto_font)
EMBED("shaders/opengl/frag.glsl", fragment_shader_source)
EMBED("shaders/opengl/solid_colour.glsl", solid_colour_fragment_shader_source)
EMBED("shaders/opengl/vert.glsl", vertex_shader_source)

static f32 scale = 1.0f;
static ImGuiStyle initial_style;
static ImFontConfig font_config;
static bool show_debug_menu = true;
static f32 target_frame_time = 0.016f;

static ShaderProgram texture_shader_program;
static ShaderProgram solid_colour_shader_program;
static u32 vertex_array_id;
static u32 vertex_buffer_id;
static u32 vertex_index_buffer_id;
static u32 last_window_height;
static u32 last_window_width;
static u32 aspect_fit_width  = 0;
static u32 aspect_fit_height = 0;
static u16 *current_tilemap = nullptr;
static Ichigo::TextureID *current_tile_texture_map = nullptr;

static Util::IchigoVector<Ichigo::Texture> textures{64};

static char string_buffer[1024];

bool Ichigo::Internal::must_rebuild_swapchain = false;
Ichigo::GameState Ichigo::game_state = {};
u32 Ichigo::Internal::current_tilemap_width  = 0;
u32 Ichigo::Internal::current_tilemap_height = 0;

struct Vertex {
    Vec3<f32> pos;
    Vec2<f32> tex;
};

struct DrawData {
    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 texture_id;
};

void default_entity_render_proc(Ichigo::Entity *entity) {
    Ichigo::Internal::gl.glUseProgram(texture_shader_program.program_id);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, textures.at(entity->texture_id).id);

    Vec2<f32> draw_pos = { entity->col.pos.x + entity->sprite_pos_offset.x - Ichigo::Camera::offset.x, entity->col.pos.y + entity->sprite_pos_offset.y - Ichigo::Camera::offset.y };
    Vertex vertices[] = {
        {{draw_pos.x, draw_pos.y, 0.0f}, {0.0f, 1.0f}},  // top left
        {{draw_pos.x + entity->sprite_w, draw_pos.y, 0.0f}, {1.0f, 1.0f}}, // top right
        {{draw_pos.x, draw_pos.y + entity->sprite_h, 0.0f}, {0.0f, 0.0f}}, // bottom left
        {{draw_pos.x + entity->sprite_w, draw_pos.y + entity->sprite_h, 0.0f}, {1.0f, 0.0f}}, // bottom right
    };

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program.program_id, "entity_texture");
    Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

u16 Ichigo::tile_at(Vec2<u32> tile_coord) {
    if (!current_tilemap || tile_coord.x >= Ichigo::Internal::current_tilemap_width || tile_coord.x < 0 || tile_coord.y >= Ichigo::Internal::current_tilemap_height || tile_coord.y < 0)
        return 0;

    return current_tilemap[tile_coord.y * Ichigo::Internal::current_tilemap_width + tile_coord.x];
}

static void render_tile(Vec2<u32> tile_pos) {
    u16 tile = Ichigo::tile_at(tile_pos);
    Vec2<f32> draw_pos = { tile_pos.x - Ichigo::Camera::offset.x, tile_pos.y - Ichigo::Camera::offset.y };
    Vertex vertices[] = {
        {{draw_pos.x, draw_pos.y, 0.0f}, {0.0f, 1.0f}},  // top left
        {{draw_pos.x + 1, draw_pos.y, 0.0f}, {1.0f, 1.0f}}, // top right
        {{draw_pos.x, draw_pos.y + 1, 0.0f}, {0.0f, 0.0f}}, // bottom left
        {{draw_pos.x + 1, draw_pos.y + 1, 0.0f}, {1.0f, 0.0f}}, // bottom right
    };

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    if (tile != 0) {
        Ichigo::Internal::gl.glUseProgram(texture_shader_program.program_id);
        Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, textures.at(current_tile_texture_map[tile]).id);

        i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program.program_id, "entity_texture");
        Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
        Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    Ichigo::Entity *player_entity = Ichigo::get_entity(Ichigo::game_state.player_entity_id);
    assert(player_entity);
    if (((tile_pos.x == player_entity->left_standing_tile.x && tile_pos.y == player_entity->left_standing_tile.y) || (tile_pos.x == player_entity->right_standing_tile.x && tile_pos.y == player_entity->right_standing_tile.y)) && FLAG_IS_SET(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
        Ichigo::Internal::gl.glUseProgram(solid_colour_shader_program.program_id);
        i32 colour_uniform = Ichigo::Internal::gl.glGetUniformLocation(solid_colour_shader_program.program_id, "colour");
        Ichigo::Internal::gl.glUniform4f(colour_uniform, 1.0f, 0.4f, tile_pos.x == player_entity->right_standing_tile.x ? 0.4f : 0.8f, 1.0f);
        Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
}

static void frame_render() {
    ImGui::Render();
    auto imgui_draw_data = ImGui::GetDrawData();
    if (imgui_draw_data->DisplaySize.x <= 0.0f || imgui_draw_data->DisplaySize.y <= 0.0f)
        return;

    // auto proj = glm::ortho(0.0f, 16.0f, 0.0f, 9.0f, -1.0f, 1.0f);
    // i32 proj_uniform = Ichigo::Internal::gl.glGetUniformLocation(shader_program_id, "proj");
    // Ichigo::Internal::gl.glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(proj));

    Ichigo::Internal::gl.glViewport(0, 0, aspect_fit_width, aspect_fit_height);
    Ichigo::Internal::gl.glClearColor(0.4f, 0.2f, 0.4f, 1.0f);
    Ichigo::Internal::gl.glClear(GL_COLOR_BUFFER_BIT);

    f32 draw_x = 0.0f;
    f32 draw_y = 0.0f;
    for (u32 row = (u32) Ichigo::Camera::offset.y; row < (u32) Ichigo::Camera::offset.y + SCREEN_TILE_HEIGHT + 1; ++row, ++draw_y) {
        draw_x = 0.0f;
        for (u32 col = (u32) Ichigo::Camera::offset.x; col < Ichigo::Camera::offset.x + SCREEN_TILE_WIDTH + 1; ++col, ++draw_x) {
            render_tile({col, row});
            // render_tile(tile_at({col, row}), {draw_x - (Ichigo::Camera::offset.x - (u32) Ichigo::Camera::offset.x), draw_y - (Ichigo::Camera::offset.y - (u32) Ichigo::Camera::offset.y)});
        }
    }

    for (u32 i = 1; i < Ichigo::Internal::entities.size; ++i) {
        Ichigo::Entity &entity = Ichigo::Internal::entities.at(i);
        if (entity.render_proc)
            entity.render_proc(&entity);
        else
            default_entity_render_proc(&entity);
    }

    // TODO: Render order? The player is now a part of the entity list. We could also skip rendering the player in the loop and render afterwards
    //       but maybe we want a more robust layering system?

    // Always render the player last (on top)
    // if (Ichigo::game_state.player_entity.render_proc)
    //     Ichigo::game_state.player_entity.render_proc(&Ichigo::game_state.player_entity);
    // else
    //     default_entity_render_proc(&Ichigo::game_state.player_entity);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Ichigo::Internal::do_frame() {
    f32 frame_start_time = Ichigo::Internal::platform_get_current_time();

    if (dt > 0.1)
        dt = 0.1;
    // static bool do_wireframe = 0;

    Ichigo::Game::frame_begin();

    if (Ichigo::Internal::window_height != last_window_height || Ichigo::Internal::window_width != last_window_width) {
        last_window_height = Ichigo::Internal::window_height;
        last_window_width  = Ichigo::Internal::window_width;
        u32 height_factor  = Ichigo::Internal::window_height / 9;
        u32 width_factor   = Ichigo::Internal::window_width  / 16;
        aspect_fit_height  = MIN(height_factor, width_factor) * 9;
        aspect_fit_width   = MIN(height_factor, width_factor) * 16;

        ICHIGO_INFO("Window resize to: %ux%u Aspect fix to: %ux%u", Ichigo::Internal::window_width, Ichigo::Internal::window_height, aspect_fit_width, aspect_fit_height);
    }

    if (dpi_scale != scale) {
        ICHIGO_INFO("scaling to scale=%f", dpi_scale);
        auto io = ImGui::GetIO();
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        io.Fonts->Clear();
        io.Fonts->AddFontFromMemoryTTF((void *) noto_font, noto_font_len, static_cast<i32>(18 * dpi_scale), &font_config, io.Fonts->GetGlyphRangesJapanese());
        io.Fonts->Build();
        ImGui_ImplOpenGL3_CreateFontsTexture();
        // Scale all Dear ImGui sizes based on the inital style
        std::memcpy(&ImGui::GetStyle(), &initial_style, sizeof(initial_style));
        ImGui::GetStyle().ScaleAllSizes(dpi_scale);
        scale = dpi_scale;
    }

    if (Ichigo::Internal::keyboard_state[Ichigo::IK_ESCAPE].down_this_frame)
        show_debug_menu = !show_debug_menu;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    if (show_debug_menu) {
        ImGui::SetNextWindowPos({0.0f, 0.0f});
        ImGui::SetNextWindowSize({Ichigo::Internal::window_width * 0.2f, (f32) Ichigo::Internal::window_height});
        ImGui::Begin("いちご！", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::Text("苺の力で...! がんばりまー");
        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            // ImGui::SeparatorText("Performance");
            ImGui::Text("FPS: %f", ImGui::GetIO().Framerate);
            ImGui::SliderFloat("Target SPF", &target_frame_time, 0.0f, 0.1f, "%fs");
            ImGui::Text("Resolution: %ux%u (%ux%u)", window_width, window_height, aspect_fit_width, aspect_fit_height);

            if (ImGui::Button("120fps"))
                target_frame_time = 0.008333f;
            if (ImGui::Button("60fps"))
                target_frame_time = 0.016f;
            if (ImGui::Button("30fps"))
                target_frame_time = 0.033f;
        }

        if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SeparatorText("Player");
            Ichigo::Entity *player_entity = Ichigo::get_entity(Ichigo::game_state.player_entity_id);
            assert(player_entity);
            ImGui::Text("player pos=%f,%f", player_entity->col.pos.x, player_entity->col.pos.y);
            ImGui::Text("player velocity=%f,%f", player_entity->velocity.x, player_entity->velocity.y);
            ImGui::Text("player on ground?=%d", FLAG_IS_SET(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND));
            ImGui::Text("left_standing_tile=%d,%d", player_entity->left_standing_tile.x, player_entity->left_standing_tile.y);
            ImGui::Text("right_standing_tile=%d,%d", player_entity->right_standing_tile.x, player_entity->right_standing_tile.y);
            ImGui::SeparatorText("Entity List");
            for (u32 i = 0; i < Ichigo::Internal::entities.size; ++i) {
                Ichigo::Entity &entity = Ichigo::Internal::entities.at(i);

                if (entity.id.index == 0) {
                    if (ImGui::TreeNode((void *) (uptr) i, "Entity slot %u: (empty)", i)) {
                        ImGui::Text("No entity loaded in this slot.");
                        ImGui::TreePop();
                    }
                } else {
                    if (ImGui::TreeNode((void *) (uptr) i, "Entity slot %u: %s (%s)", i, entity.name, Internal::entity_id_as_string(entity.id))) {
                        ImGui::PushID(entity.id.index);
                        if (ImGui::Button("Follow"))
                            Ichigo::Camera::follow(entity.id);
                        ImGui::PopID();

                        ImGui::Text("pos=%f,%f", entity.col.pos.x, entity.col.pos.y);
                        ImGui::Text("velocity=%f,%f", entity.velocity.x, entity.velocity.y);
                        ImGui::Text("on ground?=%d", FLAG_IS_SET(entity.flags, Ichigo::EntityFlag::EF_ON_GROUND));
                        ImGui::Text("left_standing_tile=%d,%d", entity.left_standing_tile.x, entity.left_standing_tile.y);
                        ImGui::Text("right_standing_tile=%d,%d", entity.right_standing_tile.x, entity.right_standing_tile.y);
                        ImGui::TreePop();
                    }
                }
            }
        }

        // ImGui::Checkbox("Wireframe", &do_wireframe);

        ImGui::End();
    }

    ImGui::EndFrame();

    // Ichigo::Internal::gl.glPolygonMode(GL_FRONT_AND_BACK, do_wireframe ? GL_LINE : GL_FILL);

    // tick_player(keyboard_state, dt);

    for (u32 i = 1; i < Ichigo::Internal::entities.size; ++i) {
        Ichigo::Entity &entity = Ichigo::Internal::entities.at(i);
        if (entity.update_proc)
            entity.update_proc(&entity);
    }

    for (u32 i = 1; i < Ichigo::Internal::entities.size; ++i) {
        Ichigo::Entity &entity = Ichigo::Internal::entities.at(i);
        for (u32 j = i; j < Ichigo::Internal::entities.size; ++j) {
            Ichigo::Entity &other_entity = Ichigo::Internal::entities.at(j);
            if (rectangles_intersect(entity.col, other_entity.col)) {
                if (entity.collide_proc)
                    entity.collide_proc(&entity, &other_entity);
                if (other_entity.collide_proc)
                    other_entity.collide_proc(&other_entity, &entity);
            }
        }
    }

    // TODO: Let the game update first? Not sure? Maybe they want to change something about an entity before it gets updated.
    //       Also we might just consider making the game call an Ichigo::update() function so that the game can control when everything updates.
    //       But also we have frame begin and frame end...?
    Ichigo::Game::update_and_render();
    Ichigo::Camera::update();

    if (Ichigo::Internal::window_height != 0 && Ichigo::Internal::window_width != 0) {
        frame_render();
    }


    f32 sleep_time = target_frame_time - (Ichigo::Internal::platform_get_current_time() - frame_start_time);
    // ICHIGO_INFO("frame start at: %f sleep time: %f", frame_start_time, sleep_time);
    if (sleep_time > 0.0f)
        Ichigo::Internal::platform_sleep(sleep_time);

    Ichigo::Game::frame_end();
}

Ichigo::TextureID Ichigo::load_texture(const u8 *png_data, u64 png_data_length) {
    TextureID new_texture_id = textures.size;
    textures.append({});
    Ichigo::Internal::gl.glGenTextures(1, &textures.at(new_texture_id).id);

    u8 *image_data = stbi_load_from_memory(png_data, png_data_length, (i32 *) &textures.at(new_texture_id).width, (i32 *) &textures.at(new_texture_id).height, (i32 *) &textures.at(new_texture_id).channel_count, 4);
    assert(image_data);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, textures.at(new_texture_id).id);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    Ichigo::Internal::gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textures.at(new_texture_id).width, textures.at(new_texture_id).height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    Ichigo::Internal::gl.glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(image_data);

    return new_texture_id;
}

void Ichigo::set_tilemap(u32 tilemap_width, u32 tilemap_height, u16 *tilemap, Ichigo::TextureID *tile_texture_map) {
    assert(tilemap_width > 0);
    assert(tilemap_height > 0);
    assert(tilemap);
    assert(tile_texture_map);

    Ichigo::Internal::current_tilemap_width  = tilemap_width;
    Ichigo::Internal::current_tilemap_height = tilemap_height;
    current_tilemap          = tilemap;
    current_tile_texture_map = tile_texture_map;
}

static void compile_shader(u32 shader_id, const GLchar *shader_source, i32 shader_source_len) {
    Ichigo::Internal::gl.glShaderSource(shader_id, 1, &shader_source, &shader_source_len);
    Ichigo::Internal::gl.glCompileShader(shader_id);

    i32 success;
    Ichigo::Internal::gl.glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);

    if (!success) {
        Ichigo::Internal::gl.glGetShaderInfoLog(shader_id, sizeof(string_buffer), nullptr, string_buffer);
        ICHIGO_ERROR("Fragment shader compilation failed:\n%s", string_buffer);
        std::exit(1);
    }
}

void Ichigo::Internal::init() {
    stbi_set_flip_vertically_on_load(true);
    font_config.FontDataOwnedByAtlas = false;
    font_config.OversampleH = 2;
    font_config.OversampleV = 2;
    font_config.RasterizerMultiply = 1.5f;

    // Init imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplOpenGL3_Init();
    initial_style = ImGui::GetStyle();

    // Fonts
    auto io = ImGui::GetIO();
    io.Fonts->AddFontFromMemoryTTF((void *) noto_font, noto_font_len, 18, &font_config, io.Fonts->GetGlyphRangesJapanese());

    ICHIGO_INFO("GL_VERSION=%s", Ichigo::Internal::gl.glGetString(GL_VERSION));

    Ichigo::Internal::gl.glEnable(GL_BLEND);
    Ichigo::Internal::gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    texture_shader_program.vertex_shader_id = Ichigo::Internal::gl.glCreateShader(GL_VERTEX_SHADER);
    solid_colour_shader_program.vertex_shader_id = texture_shader_program.vertex_shader_id;

    compile_shader(texture_shader_program.vertex_shader_id, (const GLchar *) vertex_shader_source, vertex_shader_source_len);

    texture_shader_program.fragment_shader_id = Ichigo::Internal::gl.glCreateShader(GL_FRAGMENT_SHADER);
    solid_colour_shader_program.fragment_shader_id = Ichigo::Internal::gl.glCreateShader(GL_FRAGMENT_SHADER);

    compile_shader(texture_shader_program.fragment_shader_id, (const GLchar *) fragment_shader_source, fragment_shader_source_len);
    compile_shader(solid_colour_shader_program.fragment_shader_id, (const GLchar *) solid_colour_fragment_shader_source, solid_colour_fragment_shader_source_len);

    texture_shader_program.program_id = Ichigo::Internal::gl.glCreateProgram();
    Ichigo::Internal::gl.glAttachShader(texture_shader_program.program_id, texture_shader_program.vertex_shader_id);
    Ichigo::Internal::gl.glAttachShader(texture_shader_program.program_id, texture_shader_program.fragment_shader_id);
    Ichigo::Internal::gl.glLinkProgram(texture_shader_program.program_id);

    i32 success;
    Ichigo::Internal::gl.glGetProgramiv(texture_shader_program.program_id, GL_LINK_STATUS, &success);

    if (!success) {
        Ichigo::Internal::gl.glGetProgramInfoLog(texture_shader_program.program_id, sizeof(string_buffer), nullptr, string_buffer);
        ICHIGO_ERROR("Shader program link failed:\n%s", string_buffer);
        std::exit(1);
    }

    solid_colour_shader_program.program_id = Ichigo::Internal::gl.glCreateProgram();
    Ichigo::Internal::gl.glAttachShader(solid_colour_shader_program.program_id, solid_colour_shader_program.vertex_shader_id);
    Ichigo::Internal::gl.glAttachShader(solid_colour_shader_program.program_id, solid_colour_shader_program.fragment_shader_id);
    Ichigo::Internal::gl.glLinkProgram(solid_colour_shader_program.program_id);

    Ichigo::Internal::gl.glGetProgramiv(solid_colour_shader_program.program_id, GL_LINK_STATUS, &success);

    if (!success) {
        Ichigo::Internal::gl.glGetProgramInfoLog(solid_colour_shader_program.program_id, sizeof(string_buffer), nullptr, string_buffer);
        ICHIGO_ERROR("Shader program link failed:\n%s", string_buffer);
        std::exit(1);
    }

    Ichigo::Internal::gl.glDeleteShader(texture_shader_program.vertex_shader_id);
    Ichigo::Internal::gl.glDeleteShader(texture_shader_program.fragment_shader_id);

    Ichigo::Internal::gl.glGenVertexArrays(1, &vertex_array_id);
    Ichigo::Internal::gl.glGenBuffers(1, &vertex_buffer_id);
    Ichigo::Internal::gl.glGenBuffers(1, &vertex_index_buffer_id);

    Ichigo::Internal::gl.glBindVertexArray(vertex_array_id);

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
    Ichigo::Internal::gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertex_index_buffer_id);
    Ichigo::Internal::gl.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
    Ichigo::Internal::gl.glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, tex));
    Ichigo::Internal::gl.glEnableVertexAttribArray(0);
    Ichigo::Internal::gl.glEnableVertexAttribArray(1);

    static u32 indicies[] = {
        0, 1, 2,
        1, 2, 3
    };

    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicies), indicies, GL_STATIC_DRAW);

    // The null entity
    entities.append({});
    // The null texture
    textures.append({});
    Ichigo::Game::init();
}

void Ichigo::Internal::deinit() {}
