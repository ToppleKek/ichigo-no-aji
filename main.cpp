#include <cassert>

#include "common.hpp"
#include "math.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "ichigo.hpp"

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define EMBED(FNAME, VNAME)                                                               \
    __asm__(                                                                              \
        ".section .rodata    \n"                                                          \
        ".global " #VNAME "    \n.align 16\n" #VNAME ":    \n.incbin \"" FNAME            \
        "\"       \n"                                                                     \
        ".global " #VNAME "_end\n.align 1 \n" #VNAME                                      \
        "_end:\n.byte 1                   \n"                                             \
        ".global " #VNAME "_len\n.align 16\n" #VNAME "_len:\n.int " #VNAME "_end-" #VNAME \
        "\n"                                                                              \
        ".align 16           \n.text    \n");                                             \
    alignas(16) extern const unsigned char VNAME[];                                       \
    alignas(16) extern const unsigned char *const VNAME##_end;                            \
    extern const unsigned int VNAME##_len;

extern "C" {
EMBED("noto.ttf", noto_font)
EMBED("shaders/opengl/frag.glsl", fragment_shader_source)
EMBED("shaders/opengl/vert.glsl", vertex_shader_source)
EMBED("assets/test3.png", test_png_image)
EMBED("assets/grass.png", grass_tile_png)
}

static f32 scale = 1;
static ImGuiStyle initial_style;
static ImFontConfig font_config;

static u32 vertex_shader_id;
static u32 fragment_shader_id;
static u32 shader_program_id;
static u32 vertex_array_id;
static u32 vertex_buffer_id;
static u32 vertex_index_buffer_id;
static u32 last_window_height;
static u32 last_window_width;

static char string_buffer[1024];

bool Ichigo::must_rebuild_swapchain = false;

struct Vertex {
    Vec3<f32> pos;
    Vec2<f32> tex;
};

struct Texture {
    u32 width;
    u32 height;
    u32 id;
    u32 channel_count;
    u64 png_data_size;
    const u8 *png_data;
};

struct DrawData {
    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 texture_id;
};

struct Entity {
    RectangleCollider col;
    Texture *texture;

    void render() {
        Ichigo::gl.glBindTexture(GL_TEXTURE_2D, texture->id);

        // The vertices should probably be based on the texture dimensions so that animation frames can extend outside the collider
        Vertex vertices[] = {
            {{col.pos.x, col.pos.y, 0.0f}, {0.0f, 1.0f}},  // top left
            {{col.pos.x + col.w, col.pos.y, 0.0f}, {1.0f, 1.0f}}, // top right
            {{col.pos.x, col.pos.y + col.h, 0.0f}, {0.0f, 0.0f}}, // bottom left
            {{col.pos.x + col.w, col.pos.y + col.h, 0.0f}, {1.0f, 0.0f}}, // bottom right
        };

        Ichigo::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        i32 texture_uniform = Ichigo::gl.glGetUniformLocation(shader_program_id, "entity_texture");
        Ichigo::gl.glUniform1i(texture_uniform, 0);
        Ichigo::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
};

struct Player : public Entity {
    bool on_ground;
};

#define SCREEN_TILE_WIDTH 16
#define SCREEN_TILE_HEIGHT 9

static u32 aspect_fit_width  = 0;
static u32 aspect_fit_height = 0;
static Util::IchigoVector<Entity> entities{64};
static Texture textures[Ichigo::IT_ENUM_COUNT];
static Player player_entity{{{{4.0f, 4.0f}, 1.0f, 1.0f}, &textures[Ichigo::IT_PLAYER]}, false};

static u32 tile_map[SCREEN_TILE_HEIGHT][SCREEN_TILE_WIDTH] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0},
    {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
};

static u32 tile_at(Vec2<u32> tile_coord) {
    if (tile_coord.x >= SCREEN_TILE_WIDTH || tile_coord.x < 0 || tile_coord.y >= SCREEN_TILE_HEIGHT || tile_coord.y < 0)
        return 0;

    return tile_map[tile_coord.y][tile_coord.x];
}


// x = a + t*(b-a)
static bool test_wall(f32 x, f32 x0, f32 dx, f32 *best_t) {
    // SEARCH IN T (x - x_0)/(x_1 - x_0) = t
    f32 t = safe_ratio_1(x - x0, dx);
    if (t >= 0 && t < *best_t) {
        *best_t = t;
        return true;
    }

    return false;
}

static void tick_player(Ichigo::KeyState *keyboard_state, f32 dt) {
    Vec2<f32> requested_move{0.0f, 0.0f};

#define PLAYER_SPEED 6
    if (keyboard_state[Ichigo::IK_RIGHT].down)
        requested_move.x += PLAYER_SPEED * dt;
    if (keyboard_state[Ichigo::IK_LEFT].down)
        requested_move.x -= PLAYER_SPEED * dt;
    if (keyboard_state[Ichigo::IK_DOWN].down)
        requested_move.y += PLAYER_SPEED * dt;
    if (keyboard_state[Ichigo::IK_UP].down)
        requested_move.y -= PLAYER_SPEED * dt;

    if (!player_entity.on_ground) {
        // requested_move.y += 4 * dt;
    }

    if (requested_move.x == 0.0f && requested_move.y == 0.0f) {
        return;
    }

    u32 max_tile_y = std::ceil(MAX(player_entity.col.pos.y + requested_move.y, player_entity.col.pos.y));
    u32 max_tile_x = std::ceil(MAX(player_entity.col.pos.x + requested_move.x, player_entity.col.pos.x));
    u32 min_tile_y = MIN(player_entity.col.pos.y + requested_move.y, player_entity.col.pos.y);
    u32 min_tile_x = MIN(player_entity.col.pos.x + requested_move.x, player_entity.col.pos.x);

    RectangleCollider potential_next_col = player_entity.col;
    potential_next_col.pos += requested_move;

    // ICHIGO_INFO("Nearby tiles this frame:");
    f32 best_t = 1.0;
    for (u32 tile_y = min_tile_y; tile_y <= max_tile_y; ++tile_y) {
        for (u32 tile_x = min_tile_x; tile_x <= max_tile_x; ++tile_x) {
            // ICHIGO_INFO("%u,%u", tile_x, tile_y);

            // BINARY SEARCH FOR BEST POSITION
            // if (tile_at({tile_x, tile_y}) != 0) {
            //     RectangleCollider tile_col = {{(f32) tile_x, (f32) tile_y}, 1.0f, 1.0f};
            //     if (rectangles_intersect(potential_next_col, tile_col)) {
            //         ICHIGO_INFO("Collision at tile %u,%u", tile_x, tile_y);
            //         requested_move *= 0.5;
            //         for (u32 i = 0; i < 32; ++i) {
            //             potential_next_col.pos = player_entity.col.pos + requested_move;

            //             if (rectangles_intersect(potential_next_col, tile_col)) {
            //                 requested_move *= 0.5;
            //             } else {
            //                 requested_move *= 1.5;
            //             }
            //         }
            //     }
            // }

            // SEARCH IN T (x - x_0)/(x_1 - x_0) = t
            if (tile_map[tile_y][tile_x] != 0) {
                test_wall(tile_x, (player_entity.col.pos.x + player_entity.col.w), requested_move.x, &best_t);
                test_wall(tile_x + 1, player_entity.col.pos.x, requested_move.x, &best_t);
                test_wall(tile_y, (player_entity.col.pos.y + player_entity.col.h), requested_move.y, &best_t);
                test_wall(tile_y + 1, player_entity.col.pos.y, requested_move.y, &best_t);
            }
        }
    }

    // f32 shit = requested_move.x;
    requested_move *= (best_t);
    // if (best_t < 1.0f) {
    //     ICHIGO_INFO("best_t=%f requested x=%f before=%f", best_t, requested_move.x, shit);
    // }

    player_entity.col.pos += requested_move;
}

static void render_tile(u32 tile, Vec2<f32> pos) {
    if (tile == 1) {
        Ichigo::gl.glBindTexture(GL_TEXTURE_2D, textures[Ichigo::IT_GRASS_TILE].id);

        Vertex vertices[] = {
            {{pos.x, pos.y, 0.0f}, {0.0f, 1.0f}},  // top left
            {{pos.x + 1, pos.y, 0.0f}, {1.0f, 1.0f}}, // top right
            {{pos.x, pos.y + 1, 0.0f}, {0.0f, 0.0f}}, // bottom left
            {{pos.x + 1, pos.y + 1, 0.0f}, {1.0f, 0.0f}}, // bottom right
        };

        Ichigo::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        i32 texture_uniform = Ichigo::gl.glGetUniformLocation(shader_program_id, "entity_texture");
        Ichigo::gl.glUniform1i(texture_uniform, 0);
        Ichigo::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
}

static void frame_render() {
    ImGui::Render();
    auto imgui_draw_data = ImGui::GetDrawData();
    if (imgui_draw_data->DisplaySize.x <= 0.0f || imgui_draw_data->DisplaySize.y <= 0.0f)
        return;

    // auto proj = glm::ortho(0.0f, 16.0f, 0.0f, 9.0f, -1.0f, 1.0f);
    // i32 proj_uniform = Ichigo::gl.glGetUniformLocation(shader_program_id, "proj");
    // Ichigo::gl.glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(proj));

    Ichigo::gl.glViewport(0, 0, aspect_fit_width, aspect_fit_height);
    Ichigo::gl.glClearColor(0.8f, 0.0f, 0.8f, 1.0f);
    Ichigo::gl.glClear(GL_COLOR_BUFFER_BIT);

    Ichigo::gl.glUseProgram(shader_program_id);

    for (u32 row = 0; row < SCREEN_TILE_HEIGHT; ++row) {
        for (u32 col = 0; col < SCREEN_TILE_WIDTH; ++col) {
            render_tile(tile_at({col, row}), {(f32) col, (f32) row});
        }
    }

    for (u32 i = 0; i < entities.size; ++i) {
        entities.at(i).render();
        // ICHIGO_INFO("ENTITY: pos=%f,%f", entities.at(i).pos.x, entities.at(i).pos.y);
    }

    // Always render the player last (on top)
    player_entity.render();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Ichigo::do_frame(f32 dpi_scale, f32 dt, Ichigo::KeyState *keyboard_state) {
    if (dt > 0.1)
        dt = 0.1;
    // static bool do_wireframe = 0;

    if (Ichigo::window_height != last_window_height || Ichigo::window_width != last_window_width) {
        last_window_height = Ichigo::window_height;
        last_window_width  = Ichigo::window_width;
        u32 height_factor  = Ichigo::window_height / 9;
        u32 width_factor   = Ichigo::window_width  / 16;
        aspect_fit_height  = MIN(height_factor, width_factor) * 9;
        aspect_fit_width   = MIN(height_factor, width_factor) * 16;

        ICHIGO_INFO("Window resize to: %ux%u Aspect fix to: %ux%u", Ichigo::window_width, Ichigo::window_height, aspect_fit_width, aspect_fit_height);
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
    ImGui::Text("player_pos=%f,%f", player_entity.col.pos.x, player_entity.col.pos.y);

    // ImGui::Checkbox("Wireframe", &do_wireframe);

    ImGui::End();

    ImGui::EndFrame();

    // Ichigo::gl.glPolygonMode(GL_FRONT_AND_BACK, do_wireframe ? GL_LINE : GL_FILL);

    tick_player(keyboard_state, dt);

    if (Ichigo::window_height != 0 && Ichigo::window_width != 0) {
        frame_render();
    }
}

static void load_texture(Ichigo::TextureType texture_type, const u8 *png_data, u64 png_data_length) {
    Ichigo::gl.glGenTextures(1, &textures[texture_type].id);

    u8 *image_data = stbi_load_from_memory(png_data, png_data_length, (i32 *) &textures[texture_type].width, (i32 *) &textures[texture_type].height, (i32 *) &textures[texture_type].channel_count, 4);
    assert(image_data);
    Ichigo::gl.glBindTexture(GL_TEXTURE_2D, textures[texture_type].id);
    Ichigo::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    Ichigo::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    Ichigo::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    Ichigo::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    Ichigo::gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textures[texture_type].width, textures[texture_type].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    Ichigo::gl.glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(image_data);
}

void Ichigo::init() {
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

    ICHIGO_INFO("GL_VERSION=%s", Ichigo::gl.glGetString(GL_VERSION));

    vertex_shader_id = Ichigo::gl.glCreateShader(GL_VERTEX_SHADER);

    const GLchar *pv = (const GLchar *) vertex_shader_source;
    Ichigo::gl.glShaderSource(vertex_shader_id, 1, &pv, (const i32 *) &vertex_shader_source_len);
    Ichigo::gl.glCompileShader(vertex_shader_id);

    i32 success;
    Ichigo::gl.glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &success);

    if (!success) {
        Ichigo::gl.glGetShaderInfoLog(vertex_shader_id, sizeof(string_buffer), nullptr, string_buffer);
        ICHIGO_ERROR("Vertex shader compilation failed:\n%s", string_buffer);
        std::exit(1);
    }

    fragment_shader_id = Ichigo::gl.glCreateShader(GL_FRAGMENT_SHADER);

    const GLchar *pf = (const GLchar *) fragment_shader_source;
    Ichigo::gl.glShaderSource(fragment_shader_id, 1, &pf, (const i32 *) &fragment_shader_source_len);
    Ichigo::gl.glCompileShader(fragment_shader_id);

    Ichigo::gl.glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &success);

    if (!success) {
        Ichigo::gl.glGetShaderInfoLog(fragment_shader_id, sizeof(string_buffer), nullptr, string_buffer);
        ICHIGO_ERROR("Fragment shader compilation failed:\n%s", string_buffer);
        std::exit(1);
    }

    shader_program_id = Ichigo::gl.glCreateProgram();
    Ichigo::gl.glAttachShader(shader_program_id, vertex_shader_id);
    Ichigo::gl.glAttachShader(shader_program_id, fragment_shader_id);
    Ichigo::gl.glLinkProgram(shader_program_id);

    Ichigo::gl.glGetProgramiv(shader_program_id, GL_LINK_STATUS, &success);

    if (!success) {
        Ichigo::gl.glGetProgramInfoLog(shader_program_id, sizeof(string_buffer), nullptr, string_buffer);
        ICHIGO_ERROR("Shader program link failed:\n%s", string_buffer);
        std::exit(1);
    }

    Ichigo::gl.glDeleteShader(vertex_shader_id);
    Ichigo::gl.glDeleteShader(fragment_shader_id);

    Ichigo::gl.glGenVertexArrays(1, &vertex_array_id);
    Ichigo::gl.glGenBuffers(1, &vertex_buffer_id);
    Ichigo::gl.glGenBuffers(1, &vertex_index_buffer_id);

    Ichigo::gl.glBindVertexArray(vertex_array_id);

    Ichigo::gl.glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
    Ichigo::gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertex_index_buffer_id);
    Ichigo::gl.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
    Ichigo::gl.glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, tex));
    Ichigo::gl.glEnableVertexAttribArray(0);
    Ichigo::gl.glEnableVertexAttribArray(1);

    static u32 indicies[] = {
        0, 1, 2,
        1, 2, 3
    };

    Ichigo::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicies), indicies, GL_STATIC_DRAW);

    load_texture(Ichigo::IT_PLAYER, test_png_image, test_png_image_len);
    load_texture(Ichigo::IT_GRASS_TILE, grass_tile_png, grass_tile_png_len);

    // entities.append();
    // entities.append({ {10.3f, 20.57f}, {0.0f, 0.0f, 1.0f, 0.5f}, 25.0f, 35.0f });
}

void Ichigo::deinit() {}
