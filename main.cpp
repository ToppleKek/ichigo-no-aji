#include <cassert>

#include "common.hpp"
#include "entity.hpp"
#include "math.hpp"
// #include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
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
static f32 target_frame_time = 0.016f;

static Vec2<u32> standing_tile1;
static Vec2<u32> standing_tile2;

static ShaderProgram texture_shader_program;
static ShaderProgram solid_colour_shader_program;
static u32 vertex_array_id;
static u32 vertex_buffer_id;
static u32 vertex_index_buffer_id;
static u32 last_window_height;
static u32 last_window_width;
static u32 aspect_fit_width  = 0;
static u32 aspect_fit_height = 0;
static u32 current_tilemap_width  = 0;
static u32 current_tilemap_height = 0;
static u16 *current_tilemap = nullptr;
static Ichigo::TextureID *current_tile_texture_map = nullptr;

static Util::IchigoVector<Ichigo::Texture> textures{64};

static char string_buffer[1024];

bool Ichigo::Internal::must_rebuild_swapchain = false;
Ichigo::GameState Ichigo::game_state = {};

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

    // The vertices should probably be based on the texture dimensions so that animation frames can extend outside the collider
    Vertex vertices[] = {
        {{entity->col.pos.x, entity->col.pos.y, 0.0f}, {0.0f, 1.0f}},  // top left
        {{entity->col.pos.x + entity->col.w, entity->col.pos.y, 0.0f}, {1.0f, 1.0f}}, // top right
        {{entity->col.pos.x, entity->col.pos.y + entity->col.h, 0.0f}, {0.0f, 0.0f}}, // bottom left
        {{entity->col.pos.x + entity->col.w, entity->col.pos.y + entity->col.h, 0.0f}, {1.0f, 0.0f}}, // bottom right
    };

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program.program_id, "entity_texture");
    Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

static u16 tile_at(Vec2<u32> tile_coord) {
    if (!current_tilemap || tile_coord.x >= current_tilemap_width || tile_coord.x < 0 || tile_coord.y >= current_tilemap_height || tile_coord.y < 0)
        return 0;

    return current_tilemap[tile_coord.y * current_tilemap_width + tile_coord.x];
}


// x = a + t*(b-a)
static bool test_wall(f32 x, f32 x0, f32 dx, f32 py, f32 dy, f32 ty0, f32 ty1, f32 *best_t) {
    // SEARCH IN T (x - x_0)/(x_1 - x_0) = t
    f32 t = safe_ratio_1(x - x0, dx);
    f32 y = t * dy + py;
    if (t >= 0 && t < *best_t) {
        if ((y > ty0 && y < ty1)) {
            *best_t = t;
            return true;
        } else {
            ICHIGO_INFO("y test failed t=%f x=%f x0=%f dx=%f py=%f dy=%f ty0=%f ty1=%f", t, x, x0, dx, py, dy, ty0, ty1);
        }
        // *best_t = t;
        // return true;
    // if (t >= 0 && t < *best_t) {
    }

    return false;
}

void Ichigo::EntityControllers::player_controller(Ichigo::Entity *player_entity) {
    static f32 jump_t = 0.0f;
#define PLAYER_SPEED 18.0f
#define PLAYER_FRICTION 8.0f
#define PLAYER_GRAVITY 12.0f
#define PLAYER_JUMP_ACCELERATION 128.0f

    player_entity->acceleration = {0.0f, 0.0f};
    if (Ichigo::Internal::keyboard_state[Ichigo::IK_RIGHT].down)
        player_entity->acceleration.x = PLAYER_SPEED;
    if (Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT].down)
        player_entity->acceleration.x = -PLAYER_SPEED;
    if (Ichigo::Internal::keyboard_state[Ichigo::IK_SPACE].down_this_frame && FLAG_IS_SET(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND))
        jump_t = 0.06f;

    if (jump_t != 0.0f) {
        CLEAR_FLAG(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);
        f32 effective_dt = jump_t < Ichigo::Internal::dt ? jump_t : Ichigo::Internal::dt;
        player_entity->acceleration.y = -PLAYER_JUMP_ACCELERATION * (effective_dt / Ichigo::Internal::dt);
        jump_t -= effective_dt;
    }

    i32 direction = player_entity->velocity.x < 0 ? -1 : 1;

    if (player_entity->velocity.x != 0.0f) {
        player_entity->velocity += {PLAYER_FRICTION * Ichigo::Internal::dt * -direction, 0.0f};
        i32 new_direction = player_entity->velocity.x < 0 ? -1 : 1;

        if (new_direction != direction)
            player_entity->velocity.x = 0.0f;
    }

    // p' = 1/2 at^2 + vt + p
    // v' = at + v
    // a

    // ICHIGO_INFO("dt: %f", dt);


    // u32 max_tile_y = SCREEN_TILE_HEIGHT;
    // u32 max_tile_x = SCREEN_TILE_WIDTH;
    // u32 min_tile_y = 0;
    // u32 min_tile_x = 0;

    Vec2<f32> player_delta = 0.5f * player_entity->acceleration * (Ichigo::Internal::dt * Ichigo::Internal::dt) + player_entity->velocity * Ichigo::Internal::dt;
    RectangleCollider potential_next_col = player_entity->col;
    potential_next_col.pos = player_delta + player_entity->col.pos;

    player_entity->velocity += player_entity->acceleration * Ichigo::Internal::dt;
    player_entity->velocity.x = clamp(player_entity->velocity.x, -4.0f, 4.0f);
    player_entity->velocity.y = clamp(player_entity->velocity.y, -12.0f, 12.0f);

    // player_entity.velocity.clamp(-0.08f, 0.08f);

    if (!FLAG_IS_SET(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
        player_entity->velocity.y += PLAYER_GRAVITY * Ichigo::Internal::dt;
    }

    if (player_entity->velocity.x == 0.0f && player_entity->velocity.y == 0.0f) {
        return;
    }

    // ICHIGO_INFO("Nearby tiles this frame:");
    f32 t_remaining = 1.0f;

    for (u32 i = 0; i < 4 && t_remaining > 0.0f; ++i) {
        u32 max_tile_y = std::ceil(MAX(potential_next_col.pos.y, player_entity->col.pos.y));
        u32 max_tile_x = std::ceil(MAX(potential_next_col.pos.x, player_entity->col.pos.x));
        u32 min_tile_y = MIN(potential_next_col.pos.y, player_entity->col.pos.y);
        u32 min_tile_x = MIN(potential_next_col.pos.x, player_entity->col.pos.x);
        // ICHIGO_INFO("MOVE ATTEMPT %d player velocity: %f,%f", i + 1, player_entity.velocity.x, player_entity.velocity.y);
        f32 best_t = 1.0f;
        Vec2<f32> wall_normal = { 0, 0 };

        for (u32 tile_y = min_tile_y; tile_y <= max_tile_y; ++tile_y) {
            for (u32 tile_x = min_tile_x; tile_x <= max_tile_x; ++tile_x) {
                if (tile_at({tile_x, tile_y}) != 0) {
                    Vec2<f32> centered_player_p = player_entity->col.pos + Vec2<f32>{0.5f, 0.5f};
                    Vec2<f32> min_corner = {tile_x - player_entity->col.w / 2.0f, tile_y - player_entity->col.h / 2.0f};
                    Vec2<f32> max_corner = {tile_x + 1 + player_entity->col.w / 2.0f, tile_y + 1 + player_entity->col.h / 2.0f};
                    bool updated = false;
                    if (test_wall(min_corner.x, centered_player_p.x, player_delta.x, centered_player_p.y, player_delta.y, min_corner.y, max_corner.y, &best_t)) {
                        updated = true;
                        wall_normal = { -1, 0 };
                        // ICHIGO_INFO("(-1,0) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }
                    if (test_wall(max_corner.x, centered_player_p.x, player_delta.x, centered_player_p.y, player_delta.y, min_corner.y, max_corner.y, &best_t)) {
                        updated = true;
                        wall_normal = { 1, 0 };
                        // ICHIGO_INFO("(1,0) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }
                    if (test_wall(min_corner.y, centered_player_p.y, player_delta.y, centered_player_p.x, player_delta.x, min_corner.x, max_corner.x, &best_t)) {
                        updated = true;
                        wall_normal = { 0, -1 };
                        // ICHIGO_INFO("(0,-1) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }
                    if (test_wall(max_corner.y, centered_player_p.y, player_delta.y, centered_player_p.x, player_delta.x, min_corner.x, max_corner.x, &best_t)) {
                        updated = true;
                        wall_normal = { 0, 1 };
                        // ICHIGO_INFO("(0,1) Tile collide %d,%d best_t=%f", tile_x, tile_y, best_t);
                    }

                    // if (updated)
                    //     ICHIGO_INFO("Decided wall normal: %f,%f", wall_normal.x, wall_normal.y);
                }
            }
        }

        if (wall_normal.x != 0.0f || wall_normal.y != 0.0f)
            ICHIGO_INFO("FINAL wall normal: %f,%f best_t=%f", wall_normal.x, wall_normal.y, best_t);

        if (!FLAG_IS_SET(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND) && wall_normal.y == -1.0f) {
            ICHIGO_INFO("PLAYER HIT GROUND");
            SET_FLAG(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);
        }

        player_entity->col.pos += player_delta * best_t;
        player_entity->velocity = player_entity->velocity - 1 * dot(player_entity->velocity, wall_normal) * wall_normal;
        player_delta = player_delta - 1 * dot(player_delta, wall_normal) * wall_normal;
        t_remaining -= best_t * t_remaining;

        if (player_entity->velocity.y != 0.0f)
            CLEAR_FLAG(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);

        for (u32 tile_y = min_tile_y; tile_y <= max_tile_y; ++tile_y) {
            for (u32 tile_x = min_tile_x; tile_x <= max_tile_x; ++tile_x) {
                if (tile_at({tile_x, tile_y}) != 0 && rectangles_intersect({{(f32) tile_x, (f32) tile_y}, 1.0f, 1.0f}, player_entity->col)) {
                    ICHIGO_ERROR("Collision fail at tile %d,%d", tile_x, tile_y);
                    __builtin_debugtrap();
                }
            }
        }
    }

    if (FLAG_IS_SET(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
        standing_tile1 = {(u32) player_entity->col.pos.x, (u32) player_entity->col.pos.y + 1 };
        standing_tile2 = {(u32) player_entity->col.pos.x + 1, (u32) player_entity->col.pos.y + 1 };
        if (tile_at(standing_tile1) == 0 && tile_at(standing_tile2) == 0) {
            CLEAR_FLAG(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND);
        }
    }
}

static void render_tile(u32 tile, Vec2<f32> pos) {
    if (tile != 0) {
        Ichigo::Internal::gl.glUseProgram(texture_shader_program.program_id);
        Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, textures.at(current_tile_texture_map[tile]).id);

        Vertex vertices[] = {
            {{pos.x, pos.y, 0.0f}, {0.0f, 1.0f}},  // top left
            {{pos.x + 1, pos.y, 0.0f}, {1.0f, 1.0f}}, // top right
            {{pos.x, pos.y + 1, 0.0f}, {0.0f, 0.0f}}, // bottom left
            {{pos.x + 1, pos.y + 1, 0.0f}, {1.0f, 0.0f}}, // bottom right
        };

        Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program.program_id, "entity_texture");
        Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
        Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // if (((pos.x == standing_tile1.x && pos.y == standing_tile1.y) || (pos.x == standing_tile2.x && pos.y == standing_tile2.y)) && FLAG_IS_SET(Ichigo::game_state.player_entity.flags, Ichigo::EntityFlag::EF_ON_GROUND)) {
        //     Ichigo::Internal::gl.glUseProgram(solid_colour_shader_program.program_id);
        //     i32 colour_uniform = Ichigo::Internal::gl.glGetUniformLocation(solid_colour_shader_program.program_id, "colour");
        //     Ichigo::Internal::gl.glUniform4f(colour_uniform, 1.0f, 0.4f, pos.x == standing_tile2.x ? 0.4f : 0.8f, 1.0f);
        //     Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        // }
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

    for (u32 row = 0; row < current_tilemap_width; ++row) {
        for (u32 col = 0; col < current_tilemap_width; ++col) {
            render_tile(tile_at({col, row}), {(f32) col, (f32) row});
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

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("main_window", nullptr);

    ImGui::Text("がんばりまー");
    ImGui::Text("FPS=%f", ImGui::GetIO().Framerate);
    ImGui::SliderFloat("Target frame time", &target_frame_time, 0.0f, 0.1f, "%fs");

    if (ImGui::Button("120fps"))
        target_frame_time = 0.008333f;
    if (ImGui::Button("60fps"))
        target_frame_time = 0.016f;
    if (ImGui::Button("30fps"))
        target_frame_time = 0.033f;

    Ichigo::Entity *player_entity = Ichigo::get_entity(Ichigo::game_state.player_entity_id);
    assert(player_entity);
    ImGui::Text("player pos=%f,%f", player_entity->col.pos.x, player_entity->col.pos.y);
    ImGui::Text("player velocity=%f,%f", player_entity->velocity.x, player_entity->velocity.y);
    ImGui::Text("player on ground?=%d", FLAG_IS_SET(player_entity->flags, Ichigo::EntityFlag::EF_ON_GROUND));
    ImGui::Text("standing_tile1=%d,%d", standing_tile1.x, standing_tile1.y);
    ImGui::Text("standing_tile1=%d,%d", standing_tile2.x, standing_tile2.y);

    // ImGui::Checkbox("Wireframe", &do_wireframe);

    ImGui::End();

    ImGui::EndFrame();

    // Ichigo::Internal::gl.glPolygonMode(GL_FRONT_AND_BACK, do_wireframe ? GL_LINE : GL_FILL);

    // tick_player(keyboard_state, dt);

    for (u32 i = 1; i < Ichigo::Internal::entities.size; ++i) {
        Ichigo::Entity &entity = Ichigo::Internal::entities.at(i);
        if (entity.update_proc)
            entity.update_proc(&entity);
    }

    Ichigo::Game::update_and_render();

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

    current_tilemap_width    = tilemap_width;
    current_tilemap_height   = tilemap_height;
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
    // Util::IchigoVector<Entity> fuck;
    // fuck.append({ {100.0f, 100.0f}, {0.5f, 0.2f, 0.8f}, 25, 35 });
    // fucking_shit.append({ {100.0f, 100.0f}, {0.5f, 0.2f, 0.8f}, 25, 35 });
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
