#include <cassert>

#include "camera.hpp"
#include "common.hpp"
#include "entity.hpp"
#include "math.hpp"
#include "ichigo.hpp"
#include "mixer.hpp"

#include "editor.hpp"

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_impl_opengl3.h"

#include <stb_image.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

EMBED("noto.ttf", noto_font)
EMBED("shaders/opengl/frag.glsl", fragment_shader_source)
EMBED("shaders/opengl/solid_colour.glsl", solid_colour_fragment_shader_source)
EMBED("shaders/opengl/vert.glsl", vertex_shader_source)
EMBED("shaders/opengl/screenspace_vert.glsl", screenspace_vertex_shader_source)
EMBED("assets/invalid-tile.png", invalid_tile_png)
// DEBUG
EMBED("assets/music/sound.mp3", test_sound)
static Ichigo::AudioID test_sound_id = 0;
// END DEBUG
#define MAX_DRAW_COMMANDS 4096

static f32 scale = 1.0f;
static ImGuiStyle initial_style;
static ImFontConfig font_config;
static bool show_debug_menu = true;

static GLuint texture_shader_program;
static GLuint solid_colour_shader_program;
static GLuint screenspace_solid_colour_rect_program;
static GLuint screenspace_texture_program;
static GLuint font_atlas_texture_id;

static u32 rectangle_indices[] = {
    0, 1, 2,
    1, 2, 3
};

static Ichigo::CharRange character_ranges[2] = {
    {33, 93}, // Printable ASCII
    {0x3000, 0x30FF - 0x3000} // CJK punctuation, hiragana, katakana
};

static u32 last_window_height                      = 0;
static u32 last_window_width                       = 0;
static Ichigo::Internal::ProgramMode program_mode  = Ichigo::Internal::ProgramMode::GAME;
static Ichigo::TextureID invalid_tile_texture_id;
static stbtt_packedchar *printable_ascii_pack_data;
static stbtt_packedchar *cjk_pack_data;

bool Ichigo::Internal::must_rebuild_swapchain     = false;
Ichigo::GameState Ichigo::game_state              = {};
Ichigo::Tilemap Ichigo::Internal::current_tilemap = {};
f32 Ichigo::Internal::target_frame_time           = 0.016f;
u32 Ichigo::Internal::viewport_width              = 0;
u32 Ichigo::Internal::viewport_height             = 0;
Vec2<u32> Ichigo::Internal::viewport_origin       = {};

static char string_buffer[1024];

struct TexturedVertex {
    Vec3<f32> pos;
    Vec2<f32> tex;
};

using Vertex = Vec3<f32>;

struct DrawData {
    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 vertex_index_buffer_id;
};

static DrawData draw_data_textured{};
static DrawData draw_data_solid_colour{};

static void screen_render_solid_colour_rect(Rect<f32> rect, Vec4<f32> colour) {
    Vertex vertices[] = {
        {rect.pos.x, rect.pos.y, 0.0f},  // top left
        {rect.pos.x + rect.w, rect.pos.y, 0.0f}, // top right
        {rect.pos.x, rect.pos.y + rect.h, 0.0f}, // bottom left
        {rect.pos.x + rect.w, rect.pos.y + rect.h, 0.0f}, // bottom right
    };

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_solid_colour.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_solid_colour.vertex_array_id);
    Ichigo::Internal::gl.glUseProgram(screenspace_solid_colour_rect_program);

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectangle_indices), rectangle_indices, GL_STATIC_DRAW);

    i32 colour_uniform = Ichigo::Internal::gl.glGetUniformLocation(screenspace_solid_colour_rect_program, "colour");
    Ichigo::Internal::gl.glUniform4f(colour_uniform, colour.r, colour.g, colour.b, colour.a);
    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

// static void screen_render_textured_rect(Rectangle rect, Ichigo::TextureID texture_id) {
//     TexturedVertex vertices[] = {
//         {{rect.pos.x, rect.pos.y, 0.0f}, {0.0f, 1.0f}},  // top left
//         {{rect.pos.x + rect.w, rect.pos.y, 0.0f}, {1.0f, 1.0f}}, // top right
//         {{rect.pos.x, rect.pos.y + rect.h, 0.0f}, {0.0f, 0.0f}}, // bottom left
//         {{rect.pos.x + rect.w, rect.pos.y + rect.h, 0.0f}, {1.0f, 0.0f}}, // bottom right
//     };

//     Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
//     Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);
//     Ichigo::Internal::gl.glUseProgram(screenspace_solid_colour_rect_program);
//     Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
//     i32 colour_uniform = Ichigo::Internal::gl.glGetUniformLocation(screenspace_solid_colour_rect_program, "colour");
//     Ichigo::Internal::gl.glUniform4f(colour_uniform, colour.r, colour.g, colour.b, colour.a);
//     Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
// }

static void world_render_textured_rect(Rect<f32> rect, Ichigo::TextureID texture_id) {
    Vec2<f32> draw_pos = { rect.pos.x, rect.pos.y };
    TexturedVertex vertices[] = {
        {{draw_pos.x, draw_pos.y, 0.0f}, {0.0f, 1.0f}},  // top left
        {{draw_pos.x + rect.w, draw_pos.y, 0.0f}, {1.0f, 1.0f}}, // top right
        {{draw_pos.x, draw_pos.y + rect.h, 0.0f}, {0.0f, 0.0f}}, // bottom left
        {{draw_pos.x + rect.w, draw_pos.y + rect.h, 0.0f}, {1.0f, 0.0f}}, // bottom right
    };

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, Ichigo::Internal::textures.at(texture_id).id);
    Ichigo::Internal::gl.glUseProgram(texture_shader_program);

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectangle_indices), rectangle_indices, GL_STATIC_DRAW);

    i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "entity_texture");
    i32 camera_uniform  = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "camera_transform");
    Ichigo::Internal::gl.glUniformMatrix4fv(camera_uniform, 1, GL_TRUE, (GLfloat *) &Ichigo::Camera::transform);
    Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

static void world_render_solid_colour_rect(Rect<f32> rect, Vec4<f32> colour) {
    Vec2<f32> draw_pos = { rect.pos.x, rect.pos.y };
    Vertex vertices[] = {
        {draw_pos.x, draw_pos.y, 0.0f},  // top left
        {draw_pos.x + rect.w, draw_pos.y, 0.0f}, // top right
        {draw_pos.x, draw_pos.y + rect.h, 0.0f}, // bottom left
        {draw_pos.x + rect.w, draw_pos.y + rect.h, 0.0f}, // bottom right
    };

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_solid_colour.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_solid_colour.vertex_array_id);
    Ichigo::Internal::gl.glUseProgram(solid_colour_shader_program);

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectangle_indices), rectangle_indices, GL_STATIC_DRAW);

    i32 colour_uniform = Ichigo::Internal::gl.glGetUniformLocation(solid_colour_shader_program, "colour");
    i32 camera_uniform = Ichigo::Internal::gl.glGetUniformLocation(solid_colour_shader_program, "camera_transform");
    Ichigo::Internal::gl.glUniformMatrix4fv(camera_uniform, 1, GL_TRUE, (GLfloat *) &Ichigo::Camera::transform);
    Ichigo::Internal::gl.glUniform4f(colour_uniform, colour.r, colour.g, colour.b, colour.a);
    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void default_entity_render_proc(Ichigo::Entity *entity) {
    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);
    Ichigo::Internal::gl.glUseProgram(texture_shader_program);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, Ichigo::Internal::textures.at(entity->texture_id).id);

    Vec2<f32> draw_pos = { entity->col.pos.x + entity->sprite_pos_offset.x, entity->col.pos.y + entity->sprite_pos_offset.y };
    TexturedVertex vertices[] = {
        {{draw_pos.x, draw_pos.y, 0.0f}, {0.0f, 1.0f}},  // top left
        {{draw_pos.x + entity->sprite_w, draw_pos.y, 0.0f}, {1.0f, 1.0f}}, // top right
        {{draw_pos.x, draw_pos.y + entity->sprite_h, 0.0f}, {0.0f, 0.0f}}, // bottom left
        {{draw_pos.x + entity->sprite_w, draw_pos.y + entity->sprite_h, 0.0f}, {1.0f, 0.0f}}, // bottom right
    };

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectangle_indices), rectangle_indices, GL_STATIC_DRAW);

    i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "entity_texture");
    i32 camera_uniform  = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "camera_transform");
    Ichigo::Internal::gl.glUniformMatrix4fv(camera_uniform, 1, GL_TRUE, (GLfloat *) &Ichigo::Camera::transform);
    Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void Ichigo::set_tilemap(Tilemap *tilemap) {
    assert(tilemap->width > 0 && tilemap->height > 0 && tilemap->width * tilemap->height < ICHIGO_MAX_TILEMAP_SIZE && tilemap->tile_info_count < ICHIGO_MAX_UNIQUE_TILES);
    Internal::current_tilemap.width           = tilemap->width;
    Internal::current_tilemap.height          = tilemap->height;
    Internal::current_tilemap.tile_info_count = tilemap->tile_info_count;

    std::memset(Internal::current_tilemap.tiles, 0, tilemap->width * tilemap->height * sizeof(TileID));
    std::memset(Internal::current_tilemap.tile_info, 0, tilemap->tile_info_count * sizeof(TileInfo));
    std::memcpy(Internal::current_tilemap.tiles, tilemap->tiles, tilemap->width * tilemap->height * sizeof(TileID));
    std::memcpy(Internal::current_tilemap.tile_info, tilemap->tile_info, tilemap->tile_info_count * sizeof(TileInfo));
}

#define INVALID_TILE UINT16_MAX
Ichigo::TileID Ichigo::tile_at(Vec2<u32> tile_coord) {
    if (!Internal::current_tilemap.tiles || tile_coord.x >= Internal::current_tilemap.width || tile_coord.x < 0 || tile_coord.y >= Internal::current_tilemap.height || tile_coord.y < 0)
        return INVALID_TILE;

    return Internal::current_tilemap.tiles[tile_coord.y * Internal::current_tilemap.width + tile_coord.x];
}

static void render_tile(Vec2<u32> tile_pos) {
    Ichigo::TileID tile = Ichigo::tile_at(tile_pos);
    const Ichigo::TileInfo &tile_info = Ichigo::Internal::current_tilemap.tile_info[tile];
    Vec2<f32> draw_pos = { (f32) tile_pos.x, (f32) tile_pos.y };
    TexturedVertex vertices[] = {
        {{draw_pos.x, draw_pos.y, 0.0f}, {0.0f, 1.0f}},  // top left
        {{draw_pos.x + 1, draw_pos.y, 0.0f}, {1.0f, 1.0f}}, // top right
        {{draw_pos.x, draw_pos.y + 1, 0.0f}, {0.0f, 0.0f}}, // bottom left
        {{draw_pos.x + 1, draw_pos.y + 1, 0.0f}, {1.0f, 0.0f}}, // bottom right
    };

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectangle_indices), rectangle_indices, GL_STATIC_DRAW);

    if (tile == INVALID_TILE) {
        world_render_textured_rect({{(f32) tile_pos.x, (f32) tile_pos.y}, 1.0f, 1.0f}, invalid_tile_texture_id);
    } else if (tile_info.texture_id != 0) {
        Ichigo::Internal::gl.glUseProgram(texture_shader_program);
        Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, Ichigo::Internal::textures.at(tile_info.texture_id).id);

        i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "entity_texture");
        i32 camera_uniform  = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "camera_transform");
        Ichigo::Internal::gl.glUniformMatrix4fv(camera_uniform, 1, GL_TRUE, (GLfloat *) &Ichigo::Camera::transform);
        Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
        Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
}

void Ichigo::push_draw_command(DrawCommand draw_command) {
    assert(Ichigo::game_state.this_frame_data.draw_command_count < MAX_DRAW_COMMANDS);
    Ichigo::game_state.this_frame_data.draw_commands[Ichigo::game_state.this_frame_data.draw_command_count] = draw_command;
    ++Ichigo::game_state.this_frame_data.draw_command_count;
}

f32 calculate_background_start_position(f32 camera_offset, f32 texture_width_in_metres, f32 scroll_speed) {
    // The start of the infinite looping background (in metres) is defined by:
    // begin_x = n*texture_width + offset * scroll_speed
    // And, let n be the "number of backgrounds already scrolled passed". For example, once the camera has scrolled passed one whole background,
    // n would be 1. You can think of this number telling us "what index we need to start at".
    // Imagine there are an infinite number of these backgrounds stuck together. We only ever need to draw 2 of them to cover the whole screen,
    // so the question is which 2 do we draw? Using the value for n, we know how many of these background "indices" to skip over, so we get
    // the correct x position in world space to draw the first background tile. Then, we just draw the second one after that.
    // The calculation for n is done by calculating how far the camera has to scroll to get passed one background tile.
    // This is the (texture_width_in_metres * (1 / scroll_speed)) part. If the background scrolls at half the speed of the camera (0.5f)
    // and the texture is 16.0f metres wide, then the camera must scroll 16 * (1/0.5) = 16 * 2 = 32 metres.
    return ((u32) (camera_offset / (texture_width_in_metres * (1 / scroll_speed)))) * texture_width_in_metres + camera_offset * scroll_speed;
}

static void frame_render() {
    ImGui::Render();
    auto imgui_draw_data = ImGui::GetDrawData();
    if (imgui_draw_data->DisplaySize.x <= 0.0f || imgui_draw_data->DisplaySize.y <= 0.0f)
        return;

    Ichigo::Internal::gl.glViewport(Ichigo::Internal::viewport_origin.x, Ichigo::Internal::viewport_origin.y, Ichigo::Internal::viewport_width, Ichigo::Internal::viewport_height);
    Ichigo::Internal::gl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    Ichigo::Internal::gl.glClear(GL_COLOR_BUFFER_BIT);

    // Background colour
    screen_render_solid_colour_rect({{0.0f, 0.0f}, 1.0f, 1.0f}, Ichigo::game_state.background_colour);

    // Background
    for (u32 i = 0; i < ICHIGO_MAX_BACKGROUNDS; ++i) {
        if (Ichigo::game_state.background_layers[i].texture_id != 0) {
            Ichigo::Background &bg = Ichigo::game_state.background_layers[i];
            Ichigo::Texture &bg_texture = Ichigo::Internal::textures.at(Ichigo::game_state.background_layers[i].texture_id);

            // TODO: BG_REPEAT_Y
            if (FLAG_IS_SET(bg.flags, Ichigo::BG_REPEAT_X)) {
                f32 width_metres = pixels_to_metres(bg_texture.width);
                f32 beginning_of_first_texture_in_screen = bg.start_position.x + calculate_background_start_position(-get_translation2d(Ichigo::Camera::transform).x, width_metres, bg.scroll_speed.x);
                f32 end_of_first_texture_in_screen = beginning_of_first_texture_in_screen + width_metres;

                world_render_textured_rect(
                    {
                        {beginning_of_first_texture_in_screen, bg.start_position.y + Ichigo::Camera::offset.y * bg.scroll_speed.y},
                        width_metres, pixels_to_metres(bg_texture.height)
                    },
                    bg.texture_id
                );
                world_render_textured_rect(
                    {
                        {end_of_first_texture_in_screen, bg.start_position.y + Ichigo::Camera::offset.y * bg.scroll_speed.y},
                        width_metres, pixels_to_metres(bg_texture.height)
                    },
                    bg.texture_id
                );
            } else {
                world_render_textured_rect(
                    {
                        {bg.start_position.x, bg.start_position.y},
                        pixels_to_metres(bg_texture.width), pixels_to_metres(bg_texture.height)
                    },
                    bg.texture_id
                );
            }
        }
    }

    for (i64 row = (i64) Ichigo::Camera::offset.y; row < (i64) Ichigo::Camera::offset.y + SCREEN_TILE_HEIGHT + 1; ++row) {
        for (i64 col = (i64) Ichigo::Camera::offset.x; col < Ichigo::Camera::offset.x + SCREEN_TILE_WIDTH + 1; ++col) {
            render_tile({(u32) clamp(col, (i64) 0, (i64) UINT32_MAX), (u32) clamp(row, (i64) 0, (i64) UINT32_MAX)});
        }
    }

    if (Ichigo::Camera::offset.x < 0.0f) {
        world_render_solid_colour_rect(
            {
                {Ichigo::Camera::offset.x, Ichigo::Camera::offset.y},
                std::fabsf(Ichigo::Camera::offset.x), SCREEN_TILE_HEIGHT
            },
            {0.0f, 0.0f, 0.0f, 0.5f}
        );
    }

    if (Ichigo::Camera::offset.y < 0.0f) {
        world_render_solid_colour_rect(
            {
                {Ichigo::Camera::offset.x < 0.0f ? 0.0f : Ichigo::Camera::offset.x, Ichigo::Camera::offset.y},
                SCREEN_TILE_WIDTH, std::fabsf(Ichigo::Camera::offset.y)
            },
            {0.0f, 0.0f, 0.0f, 0.5f}
        );
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

    // TODO: This is technically a "stack" I guess. Should we do this in stack order?
    for (u32 i = 0; i < Ichigo::game_state.this_frame_data.draw_command_count; ++i) {
        Ichigo::DrawCommand &cmd = Ichigo::game_state.this_frame_data.draw_commands[i];
        switch (cmd.type) {
            case Ichigo::DrawCommandType::SOLID_COLOUR_RECT: {
                world_render_solid_colour_rect(cmd.rect, cmd.colour);
            } break;

            case Ichigo::DrawCommandType::TEXT: {
                assert(Util::utf8_char_count(cmd.string, cmd.string_length) < MAX_TEXT_STRING_LENGTH);

                stbtt_fontinfo font;
                stbtt_InitFont(&font, noto_font, 0);

                Ichigo::Internal::gl.glUseProgram(screenspace_texture_program);
                Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
                Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);
                Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, font_atlas_texture_id);

                uptr            temp_ptr      = BEGIN_TEMP_MEMORY(Ichigo::game_state.transient_storage_arena);
                TexturedVertex *vertex_buffer = PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, TexturedVertex, MAX_TEXT_STRING_LENGTH * 4);
                u32            *index_buffer  = PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, u32, MAX_TEXT_STRING_LENGTH * 6);
                Vec2<f32>       current_pos   = cmd.string_pos;

                Util::BufferBuilder<TexturedVertex> vertices(vertex_buffer, MAX_TEXT_STRING_LENGTH * 4);
                Util::BufferBuilder<u32>            indices(index_buffer, MAX_TEXT_STRING_LENGTH * 6);

                for (u32 i = 0; i < cmd.string_length; ++i) {
                    if (cmd.string[i] == ' ') {
                        current_pos.x += pixels_to_metres(10.0f) * cmd.text_scale;
                        continue;
                    }

                    stbtt_packedchar pc;
                    if ((u32) cmd.string[i] >= character_ranges[0].first_codepoint && (u32) cmd.string[i] <= character_ranges[0].first_codepoint + character_ranges[0].length) {
                        pc = printable_ascii_pack_data[cmd.string[i] - character_ranges[0].first_codepoint];
                    } else if ((cmd.string[i] & 0b11100000) >> 4 == 0b1110) {
                        u32 codepoint = ((cmd.string[i] & 0b00001111) << 12) | ((cmd.string[i + 1] & 0b00111111) << 6) | ((cmd.string[i + 2] & 0b00111111));
                        if (codepoint >= character_ranges[1].first_codepoint && codepoint <= character_ranges[1].first_codepoint + character_ranges[1].length) {
                            pc = cjk_pack_data[codepoint - character_ranges[1].first_codepoint];
                        }

                        i += 2;
                    } else {
                        // This characer is not in the font atlas.
                        continue;
                    }

                    f32 u0 = pc.x0 / (f32) ICHIGO_FONT_ATLAS_DIM;
                    f32 u1 = pc.x1 / (f32) ICHIGO_FONT_ATLAS_DIM;
                    f32 v0 = pc.y0 / (f32) ICHIGO_FONT_ATLAS_DIM;
                    f32 v1 = pc.y1 / (f32) ICHIGO_FONT_ATLAS_DIM;

                    f32 x0_pixels = pc.xoff;
                    f32 x1_pixels = pc.xoff2;
                    f32 y0_pixels = pc.yoff;
                    f32 y1_pixels = pc.yoff2;

                    f32 x0 = pixels_to_metres(x0_pixels) * cmd.text_scale + current_pos.x;
                    f32 x1 = pixels_to_metres(x1_pixels) * cmd.text_scale + current_pos.x;
                    f32 y0 = pixels_to_metres(y0_pixels) * cmd.text_scale + current_pos.y;
                    f32 y1 = pixels_to_metres(y1_pixels) * cmd.text_scale + current_pos.y;

                    // 0, 1, 2,
                    // 1, 2, 3
                    u32 in[] = {
                        (u32) vertices.size,     (u32) vertices.size + 1, (u32) vertices.size + 2,
                        (u32) vertices.size + 1, (u32) vertices.size + 2, (u32) vertices.size + 3
                    };

                    indices.append(in, ARRAY_LEN(in));

                    TexturedVertex vtx[] = {
                        {{x0, y0, 0.0f}, {u0, v0}}, // top left
                        {{x1, y0, 0.0f}, {u1, v0}}, // top right
                        {{x0, y1, 0.0f}, {u0, v1}}, // bottom left
                        {{x1, y1, 0.0f}, {u1, v1}}  // bottom right
                    };

                    vertices.append(vtx, ARRAY_LEN(vtx));
                    current_pos.x += pixels_to_metres(pc.xadvance) * cmd.text_scale;
                }

                Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, vertices.size * sizeof(TexturedVertex), vertices.data, GL_STATIC_DRAW);
                Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size * sizeof(u32), indices.data, GL_STATIC_DRAW);

                i32 camera_uniform  = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "camera_transform");
                i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "entity_texture");

                Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
                Ichigo::Internal::gl.glUniformMatrix4fv(camera_uniform, 1, GL_TRUE, (GLfloat *) &Ichigo::Camera::transform);
                Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, indices.size, GL_UNSIGNED_INT, 0);

                END_TEMP_MEMORY(Ichigo::game_state.transient_storage_arena, temp_ptr);
            } break;

            default: {
                ICHIGO_ERROR("Invalid draw command type: %u", cmd.type);
            }
        }
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Ichigo::Internal::do_frame() {
    if (Ichigo::Internal::dt > 0.1)
        Ichigo::Internal::dt = 0.1;

    // static bool do_wireframe = 0;
    RESET_ARENA(Ichigo::game_state.transient_storage_arena);

    // TODO: Use an arena for this??
    Ichigo::game_state.this_frame_data.draw_commands      = PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, DrawCommand, MAX_DRAW_COMMANDS);
    Ichigo::game_state.this_frame_data.draw_command_count = 0;

    Ichigo::Game::frame_begin();

    if (Ichigo::Internal::window_height != last_window_height || Ichigo::Internal::window_width != last_window_width) {
        last_window_height                 = Ichigo::Internal::window_height;
        last_window_width                  = Ichigo::Internal::window_width;
        u32 height_factor                  = Ichigo::Internal::window_height / 9;
        u32 width_factor                   = Ichigo::Internal::window_width  / 16;
        Ichigo::Internal::viewport_height  = MIN(height_factor, width_factor) * 9;
        Ichigo::Internal::viewport_width   = MIN(height_factor, width_factor) * 16;

        Ichigo::Internal::viewport_origin = {
            (Ichigo::Internal::window_width - Ichigo::Internal::viewport_width) / 2,
            (Ichigo::Internal::window_height - Ichigo::Internal::viewport_height) / 2
        };

        ICHIGO_INFO("Window resize to: %ux%u Aspect fix to: %ux%u", Ichigo::Internal::window_width, Ichigo::Internal::window_height, Ichigo::Internal::viewport_width, Ichigo::Internal::viewport_height);
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

    if (program_mode == Ichigo::Internal::ProgramMode::EDITOR)
        Ichigo::Editor::render_ui();

    if (show_debug_menu) {
        ImGui::SetNextWindowPos({0.0f, 0.0f});
        ImGui::SetNextWindowSize({Ichigo::Internal::window_width * 0.2f, (f32) Ichigo::Internal::window_height});
        ImGui::Begin("いちご！", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::Text("苺の力で...! がんばりまー");
        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            // ImGui::SeparatorText("Performance");
            ImGui::Text("FPS: %f", ImGui::GetIO().Framerate);
            ImGui::SliderFloat("Target SPF", &Ichigo::Internal::target_frame_time, 0.0f, 0.1f, "%fs");
            ImGui::Text("Resolution: %ux%u (%ux%u)", window_width, window_height, Ichigo::Internal::viewport_width, Ichigo::Internal::viewport_height);

            if (ImGui::Button("120fps"))
                Ichigo::Internal::target_frame_time = 0.0083f;
            if (ImGui::Button("60fps"))
                Ichigo::Internal::target_frame_time = 0.016f;
            if (ImGui::Button("30fps"))
                Ichigo::Internal::target_frame_time = 0.033f;
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

        if (ImGui::CollapsingHeader("Mixer", ImGuiTreeNodeFlags_DefaultOpen)) {
            static f32 next_test_sound_volume   = 1.0f;
            static f32 next_test_sound_volume_l = 1.0f;
            static f32 next_test_sound_volume_r = 1.0f;
            ImGui::SliderFloat("Sound Volume", &next_test_sound_volume, 0.0f, 1.0f, "%f");
            ImGui::SliderFloat("Left Ch.", &next_test_sound_volume_l, 0.0f, 1.0f, "%f");
            ImGui::SliderFloat("Right Ch.", &next_test_sound_volume_r, 0.0f, 1.0f, "%f");

            if (ImGui::Button("Test Sound"))
                Ichigo::Mixer::play_audio(test_sound_id, next_test_sound_volume, next_test_sound_volume_l, next_test_sound_volume_r);

            static bool is_playing_audio = true;
            if (ImGui::Button("Toggle Playback")) {
                if (is_playing_audio) {
                    Ichigo::Internal::platform_pause_audio();
                    is_playing_audio = false;
                } else {
                    Ichigo::Internal::platform_resume_audio();
                    is_playing_audio = true;
                }
            }

            ImGui::SliderFloat("Master Volume", &Ichigo::Mixer::master_volume, 0.0f, 1.0f, "%f");

            ImGui::SeparatorText("Playing Audio");
            for (u32 i = 0; i < Mixer::playing_audio.size; ++i) {
                Mixer::PlayingAudio &pa = Mixer::playing_audio.at(i);
                if (pa.audio_id == 0) {
                    ImGui::Text("Audio slot %u: (empty)", i);
                } else {
                    if (ImGui::SmallButton("X")) {
                        Ichigo::Mixer::cancel_audio(pa.id);
                    }
                    ImGui::SameLine();
                    ImGui::Text("Audio slot %u: %u (%llu)", i, pa.audio_id, pa.frame_play_cursor);
                }
            }
        }

        if (ImGui::CollapsingHeader("Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("A: %d B: %d X: %d Y: %d", Ichigo::Internal::gamepad.a.down, Ichigo::Internal::gamepad.b.down, Ichigo::Internal::gamepad.x.down, Ichigo::Internal::gamepad.y.down);
            ImGui::Text("U: %d D: %d L: %d R: %d", Ichigo::Internal::gamepad.up.down, Ichigo::Internal::gamepad.down.down, Ichigo::Internal::gamepad.left.down, Ichigo::Internal::gamepad.right.down);
            ImGui::Text("lb: %d rb: %d", Ichigo::Internal::gamepad.lb.down, Ichigo::Internal::gamepad.rb.down);
            ImGui::Text("start: %d select: %d", Ichigo::Internal::gamepad.start.down, Ichigo::Internal::gamepad.select.down);
            ImGui::Text("lt: %f rt: %f", Ichigo::Internal::gamepad.lt, Ichigo::Internal::gamepad.rt);
            ImGui::Text("ls (click): %d rs (click): %d", Ichigo::Internal::gamepad.stick_left_click.down, Ichigo::Internal::gamepad.stick_right_click.down);
            ImGui::Text("ls: %f,%f", Ichigo::Internal::gamepad.stick_left.x, Ichigo::Internal::gamepad.stick_left.y);
            ImGui::Text("rs: %f,%f", Ichigo::Internal::gamepad.stick_right.x, Ichigo::Internal::gamepad.stick_right.y);
        }

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Camera transform:\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f",
                Ichigo::Camera::transform.a.x, Ichigo::Camera::transform.a.y, Ichigo::Camera::transform.a.z, Ichigo::Camera::transform.a.w,
                Ichigo::Camera::transform.b.x, Ichigo::Camera::transform.b.y, Ichigo::Camera::transform.b.z, Ichigo::Camera::transform.b.w,
                Ichigo::Camera::transform.c.x, Ichigo::Camera::transform.c.y, Ichigo::Camera::transform.c.z, Ichigo::Camera::transform.c.w,
                Ichigo::Camera::transform.d.x, Ichigo::Camera::transform.d.y, Ichigo::Camera::transform.d.z, Ichigo::Camera::transform.d.w
            );
            ImGui::Text("Camera offset x: %f", Ichigo::Camera::offset.x);
        }

        if (ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Position: %u,%u", Ichigo::Internal::mouse.pos.x, Ichigo::Internal::mouse.pos.y);
            ImGui::Text("Left button: %u Middle button: %u Right Button: %u", Ichigo::Internal::mouse.left_button.down, Ichigo::Internal::mouse.middle_button.down, Ichigo::Internal::mouse.right_button.down);
            ImGui::Text("Button 4: %u Button 5: %u", Ichigo::Internal::mouse.button4.down, Ichigo::Internal::mouse.button5.down);
        }

        // ImGui::Checkbox("Wireframe", &do_wireframe);

        ImGui::End();
    }

    ImGui::EndFrame();

    if (program_mode == Ichigo::Internal::ProgramMode::GAME) {

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

        if (Ichigo::Internal::keyboard_state[IK_F1].down_this_frame) {
        Ichigo::Camera::mode = Ichigo::Camera::Mode::MANUAL;
            Ichigo::Camera::manual_focus_point = Ichigo::Camera::offset;

            Ichigo::Editor::before_open();
            program_mode = Ichigo::Internal::ProgramMode::EDITOR;
        }
    } else if (program_mode == Ichigo::Internal::ProgramMode::EDITOR) {
        Ichigo::Editor::update();

        if (Ichigo::Internal::keyboard_state[IK_F1].down_this_frame) {
            Ichigo::Editor::before_close();
            Ichigo::Camera::mode = Ichigo::Camera::Mode::FOLLOW;
            program_mode = Ichigo::Internal::ProgramMode::GAME;
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

    Ichigo::Game::frame_end();
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

static GLuint link_program(GLuint vertex_shader_id, GLuint fragment_shader_id) {
    GLuint program_id = Ichigo::Internal::gl.glCreateProgram();
    Ichigo::Internal::gl.glAttachShader(program_id, vertex_shader_id);
    Ichigo::Internal::gl.glAttachShader(program_id, fragment_shader_id);
    Ichigo::Internal::gl.glLinkProgram(program_id);

    i32 success;
    Ichigo::Internal::gl.glGetProgramiv(program_id, GL_LINK_STATUS, &success);

    if (!success) {
        Ichigo::Internal::gl.glGetProgramInfoLog(program_id, sizeof(string_buffer), nullptr, string_buffer);
        ICHIGO_ERROR("Shader program link failed:\n%s", string_buffer);
        std::exit(1);
    }

    return program_id;
}

void Ichigo::Internal::init() {
    stbi_set_flip_vertically_on_load(true);

    // TODO: Platform specific allocations? VirtualAlloc() on win32
    Ichigo::game_state.transient_storage_arena.pointer  = 0;
    Ichigo::game_state.transient_storage_arena.capacity = MEGABYTES(32);
    Ichigo::game_state.transient_storage_arena.data     = (u8 *) malloc(Ichigo::game_state.transient_storage_arena.capacity);

    Ichigo::game_state.permanent_storage_arena.pointer  = 0;
    Ichigo::game_state.permanent_storage_arena.capacity = MEGABYTES(256);
    Ichigo::game_state.permanent_storage_arena.data     = (u8 *) malloc(Ichigo::game_state.permanent_storage_arena.capacity);

    Ichigo::Internal::current_tilemap.tiles     = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, TileID, ICHIGO_MAX_TILEMAP_SIZE);
    Ichigo::Internal::current_tilemap.tile_info = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, TileInfo, ICHIGO_MAX_UNIQUE_TILES);

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

    u8 *font_bitmap           = PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, u8, ICHIGO_FONT_ATLAS_DIM * ICHIGO_FONT_ATLAS_DIM);
    printable_ascii_pack_data = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, stbtt_packedchar, character_ranges[0].length);
    cjk_pack_data             = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, stbtt_packedchar, character_ranges[1].length);

    stbtt_pack_context spc = {};
    stbtt_PackBegin(&spc, font_bitmap, ICHIGO_FONT_ATLAS_DIM, ICHIGO_FONT_ATLAS_DIM, 0, 1, nullptr);

    stbtt_pack_range ranges[2] = {};

    // Printable ASCII
    ranges[0].first_unicode_codepoint_in_range = character_ranges[0].first_codepoint;
    ranges[0].num_chars                        = character_ranges[0].length;
    ranges[0].chardata_for_range               = printable_ascii_pack_data;
    ranges[0].font_size                        = 50;

    // CJK punctuation, hiragana, katakana
    ranges[1].first_unicode_codepoint_in_range = character_ranges[1].first_codepoint;
    ranges[1].num_chars                        = character_ranges[1].length;
    ranges[1].chardata_for_range               = cjk_pack_data;
    ranges[1].font_size                        = 50;

    stbtt_PackSetOversampling(&spc, 2, 2);
    stbtt_PackFontRanges(&spc, noto_font, 0, ranges, ARRAY_LEN(ranges));
    stbtt_PackEnd(&spc);

    Ichigo::Internal::gl.glGenTextures(1, &font_atlas_texture_id);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, font_atlas_texture_id);
    Ichigo::Internal::gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ICHIGO_FONT_ATLAS_DIM, ICHIGO_FONT_ATLAS_DIM, 0, GL_ALPHA, GL_UNSIGNED_BYTE, font_bitmap);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    ICHIGO_INFO("GL_VERSION=%s", Ichigo::Internal::gl.glGetString(GL_VERSION));

    Ichigo::Internal::gl.glEnable(GL_BLEND);
    Ichigo::Internal::gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint screenspace_vertex_shader    = Ichigo::Internal::gl.glCreateShader(GL_VERTEX_SHADER);
    GLuint worldspace_vertex_shader     = Ichigo::Internal::gl.glCreateShader(GL_VERTEX_SHADER);
    GLuint texture_fragment_shader      = Ichigo::Internal::gl.glCreateShader(GL_FRAGMENT_SHADER);
    GLuint solid_colour_fragment_shader = Ichigo::Internal::gl.glCreateShader(GL_FRAGMENT_SHADER);

    compile_shader(screenspace_vertex_shader, (const GLchar *) screenspace_vertex_shader_source, screenspace_vertex_shader_source_len);
    compile_shader(worldspace_vertex_shader, (const GLchar *) vertex_shader_source, vertex_shader_source_len);
    compile_shader(texture_fragment_shader, (const GLchar *) fragment_shader_source, fragment_shader_source_len);
    compile_shader(solid_colour_fragment_shader, (const GLchar *) solid_colour_fragment_shader_source, solid_colour_fragment_shader_source_len);

    texture_shader_program                = link_program(worldspace_vertex_shader, texture_fragment_shader);
    solid_colour_shader_program           = link_program(worldspace_vertex_shader, solid_colour_fragment_shader);
    screenspace_solid_colour_rect_program = link_program(screenspace_vertex_shader, solid_colour_fragment_shader);
    screenspace_texture_program           = link_program(worldspace_vertex_shader, texture_fragment_shader);

    Ichigo::Internal::gl.glDeleteShader(screenspace_vertex_shader);
    Ichigo::Internal::gl.glDeleteShader(worldspace_vertex_shader);
    Ichigo::Internal::gl.glDeleteShader(texture_fragment_shader);
    Ichigo::Internal::gl.glDeleteShader(solid_colour_fragment_shader);

    // Textured vertices
    Ichigo::Internal::gl.glGenVertexArrays(1, &draw_data_textured.vertex_array_id);
    Ichigo::Internal::gl.glGenBuffers(1, &draw_data_textured.vertex_buffer_id);
    Ichigo::Internal::gl.glGenBuffers(1, &draw_data_textured.vertex_index_buffer_id);

    Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
    Ichigo::Internal::gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw_data_textured.vertex_index_buffer_id);
    Ichigo::Internal::gl.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), 0);
    Ichigo::Internal::gl.glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), (void *) offsetof(TexturedVertex, tex));
    Ichigo::Internal::gl.glEnableVertexAttribArray(0);
    Ichigo::Internal::gl.glEnableVertexAttribArray(1);

    // Untextured (solid colour) vertices
    Ichigo::Internal::gl.glGenVertexArrays(1, &draw_data_solid_colour.vertex_array_id);
    Ichigo::Internal::gl.glGenBuffers(1, &draw_data_solid_colour.vertex_buffer_id);
    Ichigo::Internal::gl.glGenBuffers(1, &draw_data_solid_colour.vertex_index_buffer_id);

    Ichigo::Internal::gl.glBindVertexArray(draw_data_solid_colour.vertex_array_id);

    Ichigo::Internal::gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw_data_textured.vertex_index_buffer_id);
    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_solid_colour.vertex_buffer_id);
    Ichigo::Internal::gl.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
    Ichigo::Internal::gl.glEnableVertexAttribArray(0);

    // The null entity
    entities.append({});
    // The null texture
    textures.append({});
    // The null audio
    audio_assets.append({});

    invalid_tile_texture_id = Ichigo::load_texture(invalid_tile_png, invalid_tile_png_len);

    Ichigo::Game::init();

    // DEBUG
    test_sound_id = Ichigo::load_audio(test_sound, test_sound_len);
    // END DEBUG
}

void Ichigo::Internal::deinit() {}

void Ichigo::Internal::fill_sample_buffer(u8 *buffer, usize buffer_size, usize write_cursor_position_delta) {
    // assert(buffer_size % sizeof(i16) == 0);
    std::memset(buffer, 0, buffer_size);
    Ichigo::Mixer::mix_into_buffer((AudioFrame2ChI16LE *) buffer, buffer_size, write_cursor_position_delta);
    // static u8 *DEBUG_ptr = music_buffer;
    // ICHIGO_INFO("write_cursor_position_delta=%llu", write_cursor_position_delta);
    // DEBUG_ptr += write_cursor_position_delta;
    // std::memcpy(buffer, DEBUG_ptr, buffer_size);
}
