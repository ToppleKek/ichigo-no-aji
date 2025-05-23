/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Core module. Rendering, input, state, etc.

    Author:      Braeden Hong
    Last edited: 2024/12/1
*/

#include <cassert>
#include <cstddef>

#include "camera.hpp"
#include "common.hpp"
#include "entity.hpp"
#include "math.hpp"
#include "ichigo.hpp"
#include "mixer.hpp"
#include "util.hpp"

#include "editor.hpp"

#include <stb_image.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

EMBED("noto.ttf", noto_font)
EMBED("shaders/opengl/frag.glsl", fragment_shader_source)
EMBED("shaders/opengl/text.glsl", text_shader_source)
EMBED("shaders/opengl/solid_colour.glsl", solid_colour_fragment_shader_source)
EMBED("shaders/opengl/vert.glsl", vertex_shader_source)
EMBED("shaders/opengl/screenspace_vert.glsl", screenspace_vertex_shader_source)
EMBED("assets/invalid-tile.png", invalid_tile_png)

// The precomputed hash table to lookup kanji codepoints in the font atlas.
EMBED("assets/kanji.bin", kt_bytes)

static const u32 *kanji_codepoints = (u32 *) kt_bytes;
static const u32 kanji_codepoint_count = 2999;

#ifdef ICHIGO_DEBUG
#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_impl_opengl3.h"
EMBED("assets/music/sound.mp3", test_sound)
static Ichigo::AudioID test_sound_id  = 0;
static bool DEBUG_draw_colliders      = true;
static bool DEBUG_hide_entity_sprites = false;
static bool show_debug_menu           = false;
static ImGuiStyle initial_style;
static ImFontConfig font_config;
#endif

#define MAX_DRAW_COMMANDS 4096

static f32 last_dpi_scale = 1.0f;
static GLuint texture_shader_program;
static GLuint text_shader_program;
static GLuint screenspace_text_shader_program; // FIXME: All these "screenspace" variants suck
static GLuint solid_colour_shader_program;
static GLuint screenspace_solid_colour_rect_program;
static GLuint screenspace_texture_program;
static GLuint font_atlas_texture_id;

// NOTE: The vertex indices for drawing a rectangle.
static u32 rectangle_indices[] = {
    0, 1, 2,
    1, 2, 3
};

static Ichigo::CharRange character_ranges[] = {
    {33, 93}, // Printable ASCII
    {0x3000, 0x30FF - 0x3000}, // CJK punctuation, hiragana, katakana
    {0x4E00, 0x9FFF - 0x4E00}, // CJK unified ideographs (we only care about/support a subset of this!)
    {0xFF00, 0xFFEF - 0xFF00}, // Halfwidth and fullwidth forms
};

static u32 last_window_height                     = 0;
static u32 last_window_width                      = 0;
static Ichigo::Internal::ProgramMode program_mode = Ichigo::Internal::ProgramMode::GAME;

// Invalid tile texture. Shown in the editor when you pan the camera passed the bounds of the tilemap.
static Ichigo::TextureID invalid_tile_texture_id;

// Data for looking up data in the font atlas.
static stbtt_packedchar *printable_ascii_pack_data;
static stbtt_packedchar *cjk_pack_data;
static stbtt_packedchar *kanji_pack_data;
static stbtt_packedchar *half_and_full_width_pack_data;

static Mat4<f32> identity_mat4;

#define INFO_LOG_MAX_BYTE_LENGTH 256
#define INFO_LOG_MAX_MESSAGES 8

// Global state that is visible in ichigo.hpp
bool Ichigo::Internal::must_rebuild_swapchain     = false;
Ichigo::GameState Ichigo::game_state              = {};
Ichigo::Tilemap Ichigo::Internal::current_tilemap = {};
f32 Ichigo::Internal::target_frame_time           = 0.016f;
u32 Ichigo::Internal::viewport_width              = 0;
u32 Ichigo::Internal::viewport_height             = 0;
Vec2<u32> Ichigo::Internal::viewport_origin       = {};

static void *temp_arena_alloc(usize size) {
    return (void *) PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, u8, size);
}

static void arena_free_nop(void *) {}

static void *perm_arena_alloc(usize size) {
    return (void *) PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, u8, size);
}

Bana::Allocator Ichigo::Internal::temp_allocator = {
    temp_arena_alloc,
    arena_free_nop
};

Bana::Allocator Ichigo::Internal::perm_allocator = {
    perm_arena_alloc,
    arena_free_nop
};

// General purpose static string buffer.
static char string_buffer[1024];

// Some state for OpenGL
struct DrawData {
    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 vertex_index_buffer_id;
};

// A message in the info log. The info log is the message log that is shown in the center of the screen.
// Eg. in the editor it shows "Selected: <tile>"
struct InfoLogMessage {
    char *data;
    u32 length;
    f32 t_remaining;
};

// The precomputed hashmap interface. This also could just be a function. No idea why it's in a struct actually.
struct KanjiHashMap {
    const u32 *data;
    u32 codepoint_count;

    i64 get(u32 codepoint) {
        for (u32 i = codepoint % codepoint_count, j = 0; j < codepoint_count; i = (i + 1) % codepoint_count, ++j) {
            if (data[i] == codepoint) {
                return i;
            }
        }

        return -1;
    }
};

static KanjiHashMap kanji_hash_map;

static DrawData draw_data_textured{};
static DrawData draw_data_solid_colour{};

// Render a solid colour rectangle in screenspace.
static void screen_render_solid_colour_rect(Rect<f32> rect, Vec4<f32> colour, Mat4<f32> transform = m4identity_f32) {
    Vertex vertices[] = {
        {rect.pos.x, rect.pos.y, 0.0f},  // top left
        {rect.pos.x + rect.w, rect.pos.y, 0.0f}, // top right
        {rect.pos.x, rect.pos.y + rect.h, 0.0f}, // bottom left
        {rect.pos.x + rect.w, rect.pos.y + rect.h, 0.0f}, // bottom right
    };

    Ichigo::Internal::gl.glUseProgram(screenspace_solid_colour_rect_program);

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_solid_colour.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_solid_colour.vertex_array_id);

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectangle_indices), rectangle_indices, GL_STATIC_DRAW);

    i32 colour_uniform = Ichigo::Internal::gl.glGetUniformLocation(screenspace_solid_colour_rect_program, "colour");
    i32 object_uniform = Ichigo::Internal::gl.glGetUniformLocation(screenspace_solid_colour_rect_program, "object_transform");

    Ichigo::Internal::gl.glUniform4f(colour_uniform, colour.r, colour.g, colour.b, colour.a);
    Ichigo::Internal::gl.glUniformMatrix4fv(object_uniform, 1, GL_TRUE, (GLfloat *) &transform);

    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

// Render a textured rectangle in screenspace.
static void screen_render_textured_rect(Rect<f32> rect, Ichigo::TextureID texture_id, Mat4<f32> transform = m4identity_f32) {
    Vec2<f32> draw_pos = { rect.pos.x, rect.pos.y };
    TexturedVertex vertices[] = {
        {{draw_pos.x, draw_pos.y, 0.0f}, {0.0f, 1.0f}},  // top left
        {{draw_pos.x + rect.w, draw_pos.y, 0.0f}, {1.0f, 1.0f}}, // top right
        {{draw_pos.x, draw_pos.y + rect.h, 0.0f}, {0.0f, 0.0f}}, // bottom left
        {{draw_pos.x + rect.w, draw_pos.y + rect.h, 0.0f}, {1.0f, 0.0f}}, // bottom right
    };

    Ichigo::Internal::gl.glUseProgram(screenspace_texture_program);

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, Ichigo::Internal::textures[texture_id].id);

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectangle_indices), rectangle_indices, GL_STATIC_DRAW);

    i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "entity_texture");
    i32 object_uniform  = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "object_transform");
    i32 tint_uniform    = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "colour_tint");

    Ichigo::Internal::gl.glUniform4f(tint_uniform, 1.0f, 1.0f, 1.0f, 1.0f);
    Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
    Ichigo::Internal::gl.glUniformMatrix4fv(object_uniform, 1, GL_TRUE, (GLfloat *) &transform);

    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

// Render a textured rectangle in worldspace.
void Ichigo::world_render_textured_rect(Rect<f32> rect, Ichigo::TextureID texture_id, Mat4<f32> transform, Vec4<f32> tint) {
    Vec2<f32> draw_pos = { rect.pos.x, rect.pos.y };
    transform = translate2d(draw_pos) * transform;

    TexturedVertex vertices[] = {
        {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},  // top left
        {{rect.w, 0.0f, 0.0f}, {1.0f, 1.0f}}, // top right
        {{0.0f, rect.h, 0.0f}, {0.0f, 0.0f}}, // bottom left
        {{rect.w, rect.h, 0.0f}, {1.0f, 0.0f}}, // bottom right
    };

    Ichigo::Internal::gl.glUseProgram(texture_shader_program);

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, Ichigo::Internal::textures[texture_id].id);

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectangle_indices), rectangle_indices, GL_STATIC_DRAW);

    i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "entity_texture");
    i32 object_uniform  = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "object_transform");
    i32 tint_uniform    = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "colour_tint");

    Ichigo::Internal::gl.glUniform4fv(tint_uniform, 1, (GLfloat *) &tint);
    Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
    Ichigo::Internal::gl.glUniformMatrix4fv(object_uniform, 1, GL_TRUE, (GLfloat *) &transform);

    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

// Render a solid colour rectangle in worldspace.
void Ichigo::world_render_solid_colour_rect(Rect<f32> rect, Vec4<f32> colour, Mat4<f32> transform) {
    Vec2<f32> draw_pos = { rect.pos.x, rect.pos.y };
    transform = translate2d(draw_pos) * transform;

    Vertex vertices[] = {
        {0.0f, 0.0f, 0.0f},     // top left
        {rect.w, 0.0f, 0.0f},   // top right
        {0.0f, rect.h, 0.0f},   // bottom left
        {rect.w, rect.h, 0.0f}, // bottom right
    };

    Ichigo::Internal::gl.glUseProgram(solid_colour_shader_program);

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_solid_colour.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_solid_colour.vertex_array_id);

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectangle_indices), rectangle_indices, GL_STATIC_DRAW);

    i32 colour_uniform = Ichigo::Internal::gl.glGetUniformLocation(solid_colour_shader_program, "colour");
    i32 object_uniform = Ichigo::Internal::gl.glGetUniformLocation(solid_colour_shader_program, "object_transform");

    Ichigo::Internal::gl.glUniform4f(colour_uniform, colour.r, colour.g, colour.b, colour.a);
    Ichigo::Internal::gl.glUniformMatrix4fv(object_uniform, 1, GL_TRUE, (GLfloat *) &transform);

    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void Ichigo::world_render_rect_list(Vec2<f32> pos, const Bana::FixedArray<TexturedRect> &rects, TextureID texture_id, Mat4<f32> transform, Vec4<f32> tint) {
    transform = translate2d(pos) * transform;

    uptr temp_ptr = BEGIN_TEMP_MEMORY(Ichigo::game_state.transient_storage_arena);

    Bana::FixedArray<TexturedVertex> vtx = Bana::make_fixed_array<TexturedVertex>(rects.size * 4, Ichigo::Internal::temp_allocator);
    Bana::FixedArray<u32>            idx = Bana::make_fixed_array<u32>(rects.size * 6, Ichigo::Internal::temp_allocator);
    Texture                          tex = Ichigo::Internal::textures[texture_id];

    for (u32 i = 0; i < rects.size; ++i) {
        u32 in[] = {
            (u32) vtx.size,     (u32) vtx.size + 1, (u32) vtx.size + 2,
            (u32) vtx.size + 1, (u32) vtx.size + 2, (u32) vtx.size + 3
        };

        idx.append(in, ARRAY_LEN(in));

        f32 x = rects[i].draw_rect.pos.x;
        f32 y = rects[i].draw_rect.pos.y;
        f32 w = rects[i].draw_rect.w;
        f32 h = rects[i].draw_rect.h;
        f32 tex_x = rects[i].texture_rect.pos.x;
        f32 tex_y = rects[i].texture_rect.pos.y;
        f32 tex_w = rects[i].texture_rect.w;
        f32 tex_h = rects[i].texture_rect.h;

        // UV coordinates.
        f32 u0 = (f32) tex_x / (f32) tex.width;
        f32 v0 = (f32) (tex.height - tex_y) / (f32) tex.height;
        f32 u1 = u0 + (tex_w / (f32) tex.width);
        f32 v1 = v0 - (tex_h / (f32) tex.height);

        TexturedVertex vt[] = {
            {{x, y, 0.0f},         {u0, v0}}, // top left
            {{x + w, y, 0.0f},     {u1, v0}}, // top right
            {{x, y + h, 0.0f},     {u0, v1}}, // bottom left
            {{x + w, y + h, 0.0f}, {u1, v1}}, // bottom right
        };

        vtx.append(vt, ARRAY_LEN(vt));
    }

    Ichigo::Internal::gl.glUseProgram(texture_shader_program);

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, tex.id);

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, vtx.size * sizeof(TexturedVertex), vtx.data, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size * sizeof(u32), idx.data, GL_STATIC_DRAW);

    i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "entity_texture");
    i32 object_uniform  = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "object_transform");
    i32 tint_uniform    = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "colour_tint");

    Ichigo::Internal::gl.glUniform4fv(tint_uniform, 1, (GLfloat *) &tint);
    Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
    Ichigo::Internal::gl.glUniformMatrix4fv(object_uniform, 1, GL_TRUE, (GLfloat *) &transform);

    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, idx.size, GL_UNSIGNED_INT, 0);

    END_TEMP_MEMORY(Ichigo::game_state.transient_storage_arena, temp_ptr);
}

void Ichigo::render_sprite_and_advance_animation(CoordinateSystem coordinate_system, Vec2<f32> pos, Sprite *sprite, bool flip_h, bool actually_render, Mat4<f32> transform, Vec4<f32> tint) {
    const Ichigo::Texture &texture = Ichigo::Internal::textures[sprite->sheet.texture];

    Ichigo::Internal::gl.glUseProgram(texture_shader_program);
    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, texture.id);

    // Calculate the current animation frame cell and stuff.
    u32 current_cell  = sprite->animation.cell_of_first_frame + sprite->current_animation_frame;
    u32 cells_per_row = texture.width / sprite->sheet.cell_width;
    u32 row_of_cell   = current_cell  / cells_per_row;
    u32 col_of_cell   = current_cell  % cells_per_row;
    u32 cell_x_px     = col_of_cell   * sprite->sheet.cell_width;
    u32 cell_y_px     = row_of_cell   * sprite->sheet.cell_height;

    // UV coordinates.
    f32 u0 = (f32) cell_x_px / (f32) texture.width;
    f32 v0 = (f32) (texture.height - cell_y_px) / (f32) texture.height;
    f32 u1 = u0 + ((f32) sprite->sheet.cell_width  / (f32) texture.width);
    f32 v1 = v0 - ((f32) sprite->sheet.cell_height / (f32) texture.height);

    if (flip_h) {
        f32 temp = u0;
        u0       = u1;
        u1       = temp;
    }

    // Advance the animation.
    Ichigo::Animation &anim = sprite->animation;

    if (anim.cell_of_first_frame != anim.cell_of_last_frame) {
        sprite->elapsed_animation_frame_time += Ichigo::Internal::dt;
    }

    // Skip animation frames if we are running too slow.
    // FIXME: If we skip the final animation frame, then gameplay code that depends on the ending frame being hit will break. Maybe there should be a "ANIM_PLAYED_ONCE" flag or something.
    u32 frames_advanced = (u32) safe_ratio_0(sprite->elapsed_animation_frame_time, anim.seconds_per_frame);

    if (frames_advanced > 0) {
        sprite->elapsed_animation_frame_time -= (frames_advanced * anim.seconds_per_frame);
        u32 anim_duration = anim.cell_of_last_frame - anim.cell_of_first_frame + 1;

        if (sprite->current_animation_frame + frames_advanced >= anim_duration) {
            sprite->current_animation_frame = anim.cell_of_loop_start - anim.cell_of_first_frame;
        } else {
            sprite->current_animation_frame += frames_advanced;
        }
    }

    // NOTE: This is here so that if an entity is marked as invisible it can still advance animation frames.
    if (!actually_render) return;

    i32 screen_dimension_uniform = Ichigo::Internal::gl.glGetUniformLocation(text_shader_program, "screen_tile_dimensions");
    i32 texture_uniform          = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "entity_texture");
    i32 object_uniform           = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "object_transform");
    i32 tint_uniform             = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "colour_tint");

    Vec2<f32> translation;

    if (coordinate_system == Ichigo::CoordinateSystem::CAMERA) {
        translation = pos - get_translation2d(Ichigo::Camera::transform);
    } else if (coordinate_system == Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX) {
        translation = pos - get_translation2d(Ichigo::Camera::transform);
        Ichigo::Internal::gl.glUniform2f(screen_dimension_uniform, (f32) SCREEN_ASPECT_FIX_WIDTH, (f32) SCREEN_ASPECT_FIX_HEIGHT);
    } else {
        translation = pos + sprite->pos_offset;
    }

    transform = translate2d(translation) * transform;

    TexturedVertex vertices[] = {
        {{0.0f, 0.0f, 0.0f},                    {u0, v0}}, // top left
        {{sprite->width, 0.0f, 0.0f},           {u1, v0}}, // top right
        {{0.0f, sprite->height, 0.0f},          {u0, v1}}, // bottom left
        {{sprite->width, sprite->height, 0.0f}, {u1, v1}}, // bottom right
    };

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectangle_indices), rectangle_indices, GL_STATIC_DRAW);

    Ichigo::Internal::gl.glUniform4fv(tint_uniform, 1, (GLfloat *) &tint);

    Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
    Ichigo::Internal::gl.glUniformMatrix4fv(object_uniform, 1, GL_TRUE, (GLfloat *) &transform);

    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

f32 Ichigo::get_text_width(const char *str, usize length, Ichigo::TextStyle style) {
    f32 width = 0.0f;
    const u8 *unsigned_str = (const u8 *) str;
    for (u32 i = 0; i < length;) {
        // TODO: Handle \n and return the max of width.
        if (unsigned_str[i] == ' ') {
            width += 10.0f * style.scale;
            ++i;
            continue;
        }

        // TODO: Factor this out so both functions can use the same code?
        stbtt_packedchar pc;
        u32 codepoint = 0;
        if (unsigned_str[i] <= 0x7F) {
            codepoint = unsigned_str[i];
            ++i;
        } else if ((unsigned_str[i] & 0b11110000) == 0b11110000) {
            codepoint = ((unsigned_str[i] & 0b00000111) << 18) | ((unsigned_str[i + 1] & 0b00111111) << 12) | ((unsigned_str[i + 2] & 0b00111111) << 6) | (unsigned_str[i + 3] & 0b00111111);
            i += 4;
        } else if ((unsigned_str[i] & 0b11100000) == 0b11100000) {
            codepoint = ((unsigned_str[i] & 0b00001111) << 12) | ((unsigned_str[i + 1] & 0b00111111) << 6) | ((unsigned_str[i + 2] & 0b00111111));
            i += 3;
        } else if ((unsigned_str[i] & 0b11000000) == 0b11000000) {
            codepoint = ((unsigned_str[i] & 0b00011111) << 6) | ((unsigned_str[i + 1] & 0b00111111));
            i += 2;
        } else {
            ICHIGO_ERROR("Not a valid utf8 character: %u", unsigned_str[i]);
            ++i;
            continue;
        }

        if (codepoint >= character_ranges[0].first_codepoint && codepoint <= character_ranges[0].first_codepoint + character_ranges[0].length) {
            pc = printable_ascii_pack_data[codepoint - character_ranges[0].first_codepoint];
        } else if (codepoint >= character_ranges[1].first_codepoint && codepoint <= character_ranges[1].first_codepoint + character_ranges[1].length) {
            pc = cjk_pack_data[codepoint - character_ranges[1].first_codepoint];
        } else if (codepoint >= character_ranges[2].first_codepoint && codepoint <= character_ranges[2].first_codepoint + character_ranges[2].length) {
            i64 index = kanji_hash_map.get(codepoint);
            if (index == -1) {
                ICHIGO_ERROR("Unsupported CJK ideograph: 0x%X", codepoint);
                continue;
            }

            pc = kanji_pack_data[index];
        } else if (codepoint >= character_ranges[3].first_codepoint && codepoint <= character_ranges[3].first_codepoint + character_ranges[3].length) {
            pc = half_and_full_width_pack_data[codepoint - character_ranges[3].first_codepoint];
        } else {
            // This characer is not in the font atlas.
            continue;
        }

        width += pc.xadvance * style.scale;
    }

    return width * pixels_to_metres(0.2f);
}

void Ichigo::render_text(Vec2<f32> pos, const char *str, usize length, Ichigo::CoordinateSystem coordinate_system, Ichigo::TextStyle style) {
    assert(Util::utf8_char_count(str, length) < MAX_TEXT_STRING_LENGTH);

    // Use some transient memory temporarily :)
    uptr            temp_ptr      = BEGIN_TEMP_MEMORY(Ichigo::game_state.transient_storage_arena);
    TexturedVertex *vertex_buffer = PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, TexturedVertex, MAX_TEXT_STRING_LENGTH * 4);
    u32            *index_buffer  = PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, u32, MAX_TEXT_STRING_LENGTH * 6);
    Vec2<f32>       current_pos   = {0.0f, 0.0f};

    // Construct a vertex and index buffer to upload to the GPU for rendering.
    Bana::BufferBuilder<TexturedVertex> vertices(vertex_buffer, MAX_TEXT_STRING_LENGTH * 4);
    Bana::BufferBuilder<u32>            indices(index_buffer, MAX_TEXT_STRING_LENGTH * 6);

    const u8 *unsigned_str = (const u8 *) str;
    for (u32 i = 0; i < length;) {
        if (unsigned_str[i] == ' ') {
            current_pos.x += 10.0f * style.scale;
            ++i;
            continue;
        }

        if (unsigned_str[i] == '\n') {
            current_pos.y += style.line_spacing * style.scale;
            current_pos.x = 0.0f;
            ++i;
            continue;
        }

        // Decode UTF8 encoded data into unicode codepoints.
        stbtt_packedchar pc;
        u32 codepoint = 0;
        if (unsigned_str[i] <= 0x7F) {
            codepoint = unsigned_str[i];
            ++i;
        } else if ((unsigned_str[i] & 0b11110000) == 0b11110000) {
            codepoint = ((unsigned_str[i] & 0b00000111) << 18) | ((unsigned_str[i + 1] & 0b00111111) << 12) | ((unsigned_str[i + 2] & 0b00111111) << 6) | (unsigned_str[i + 3] & 0b00111111);
            i += 4;
        } else if ((unsigned_str[i] & 0b11100000) == 0b11100000) {
            codepoint = ((unsigned_str[i] & 0b00001111) << 12) | ((unsigned_str[i + 1] & 0b00111111) << 6) | ((unsigned_str[i + 2] & 0b00111111));
            i += 3;
        } else if ((unsigned_str[i] & 0b11000000) == 0b11000000) {
            codepoint = ((unsigned_str[i] & 0b00011111) << 6) | ((unsigned_str[i + 1] & 0b00111111));
            i += 2;
        } else {
            ICHIGO_ERROR("Not a valid utf8 character: %u", unsigned_str[i]);
            ++i;
            continue;
        }

        // Get the right pack data to be able to get the right font atlas coordinates.
        if (codepoint >= character_ranges[0].first_codepoint && codepoint <= character_ranges[0].first_codepoint + character_ranges[0].length) {
            pc = printable_ascii_pack_data[codepoint - character_ranges[0].first_codepoint];
        } else if (codepoint >= character_ranges[1].first_codepoint && codepoint <= character_ranges[1].first_codepoint + character_ranges[1].length) {
            pc = cjk_pack_data[codepoint - character_ranges[1].first_codepoint];
        } else if (codepoint >= character_ranges[2].first_codepoint && codepoint <= character_ranges[2].first_codepoint + character_ranges[2].length) {
            i64 index = kanji_hash_map.get(codepoint);
            if (index == -1) {
                ICHIGO_ERROR("Unsupported CJK ideograph: 0x%X", codepoint);
                continue;
            }

            pc = kanji_pack_data[index];
        } else if (codepoint >= character_ranges[3].first_codepoint && codepoint <= character_ranges[3].first_codepoint + character_ranges[3].length) {
            pc = half_and_full_width_pack_data[codepoint - character_ranges[3].first_codepoint];
        } else {
            // This characer is not in the font atlas.
            continue;
        }

        // Calculate the uv coordinates and the xy coordinates of the text.
        // NOTE: The xy coordinates are all done in unscaled "font units". These values are all scaled and translated in the text shader according to the coordinate system specified.
        f32 u0 = pc.x0 / (f32) ICHIGO_FONT_ATLAS_WIDTH;
        f32 u1 = pc.x1 / (f32) ICHIGO_FONT_ATLAS_WIDTH;
        f32 v0 = pc.y0 / (f32) ICHIGO_FONT_ATLAS_HEIGHT;
        f32 v1 = pc.y1 / (f32) ICHIGO_FONT_ATLAS_HEIGHT;

        f32 x0_pixels = pc.xoff;
        f32 x1_pixels = pc.xoff2;
        f32 y0_pixels = pc.yoff;
        f32 y1_pixels = pc.yoff2;

        f32 x0 = x0_pixels * style.scale + current_pos.x;
        f32 x1 = x1_pixels * style.scale + current_pos.x;
        f32 y0 = y0_pixels * style.scale + current_pos.y;
        f32 y1 = y1_pixels * style.scale + current_pos.y;

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
        current_pos.x += pc.xadvance * style.scale;
    }

    f32 line_width = current_pos.x;

    // How much do we need to adjust the x coordinate to align the text correctly?
    f32 x_offset = 0.0f;
    switch (style.alignment) {
        case Ichigo::TextAlignment::CENTER: {
            x_offset = -line_width / 2.0f;
        } break;

        case Ichigo::TextAlignment::RIGHT: {
            x_offset = -line_width;
        } break;

        default:; // Fallthrough. Left alignment doesn't need any adjustments.
    }

    i32 texture_uniform;
    i32 colour_uniform;
    i32 alpha_adjust_uniform;
    i32 screen_dimension_uniform = Ichigo::Internal::gl.glGetUniformLocation(text_shader_program, "screen_tile_dimensions");

    switch (coordinate_system) { // FIXME: Remove screenspace??
        case Ichigo::CoordinateSystem::CAMERA:
        case Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX:
        case Ichigo::CoordinateSystem::WORLD: {
            Ichigo::Internal::gl.glUseProgram(text_shader_program);
            Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
            Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);
            Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, font_atlas_texture_id);

            Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, vertices.size * sizeof(TexturedVertex), vertices.data, GL_STATIC_DRAW);
            Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size * sizeof(u32), indices.data, GL_STATIC_DRAW);

            Vec2<f32> translation;

            // If the coordinate system is CAMERA, the only difference is we have to consider the location of the camera in the final translation.
            if (coordinate_system == Ichigo::CoordinateSystem::CAMERA) {
                translation = pos - get_translation2d(Ichigo::Camera::transform);
            } else if (coordinate_system == Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX) {
                translation = pos - get_translation2d(Ichigo::Camera::transform);
                Ichigo::Internal::gl.glUniform2f(screen_dimension_uniform, (f32) SCREEN_ASPECT_FIX_WIDTH, (f32) SCREEN_ASPECT_FIX_HEIGHT);
            } else {
                translation = pos;
            }

            // Apply the alignment offset.
            translation.x += pixels_to_metres(0.2f) * x_offset;

            // Set the translate and scale transforms to convert the "font units" to world metres.
            i32 object_uniform       = Ichigo::Internal::gl.glGetUniformLocation(text_shader_program, "object_transform");
            Mat4<f32> text_transform = translate2d(translation) * scale2d({pixels_to_metres(0.2f), pixels_to_metres(0.2f)});

            Ichigo::Internal::gl.glUniformMatrix4fv(object_uniform, 1, GL_TRUE, (GLfloat *) &text_transform);

            texture_uniform      = Ichigo::Internal::gl.glGetUniformLocation(text_shader_program, "font_atlas");
            colour_uniform       = Ichigo::Internal::gl.glGetUniformLocation(text_shader_program, "colour");
            alpha_adjust_uniform = Ichigo::Internal::gl.glGetUniformLocation(text_shader_program, "alpha_adjust");
        } break;

        case Ichigo::CoordinateSystem::SCREEN: {
            Ichigo::Internal::gl.glUseProgram(screenspace_text_shader_program);
            Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
            Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);
            Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, font_atlas_texture_id);

            Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, vertices.size * sizeof(TexturedVertex), vertices.data, GL_STATIC_DRAW);
            Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size * sizeof(u32), indices.data, GL_STATIC_DRAW);

            Vec2<f32> translation = pos;
            translation.x += x_offset / Ichigo::Internal::viewport_width;

            i32 object_uniform       = Ichigo::Internal::gl.glGetUniformLocation(screenspace_text_shader_program, "object_transform");
            Mat4<f32> text_transform = translate2d(pos) * scale2d({1.0f / Ichigo::Internal::viewport_width, 1.0f / Ichigo::Internal::viewport_height});

            Ichigo::Internal::gl.glUniformMatrix4fv(object_uniform, 1, GL_TRUE, (GLfloat *) &text_transform);

            texture_uniform      = Ichigo::Internal::gl.glGetUniformLocation(screenspace_text_shader_program, "font_atlas");
            colour_uniform       = Ichigo::Internal::gl.glGetUniformLocation(screenspace_text_shader_program, "colour");
            alpha_adjust_uniform = Ichigo::Internal::gl.glGetUniformLocation(screenspace_text_shader_program, "alpha_adjust");
        } break;

        default: {
            ICHIGO_ERROR("Invalid coordinate system specified for text rendering!");
            return;
        }
    }


    Ichigo::Internal::gl.glUniform3f(colour_uniform, style.colour.r, style.colour.g, style.colour.b);
    Ichigo::Internal::gl.glUniform1f(alpha_adjust_uniform, style.colour.a);
    Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);

    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, indices.size, GL_UNSIGNED_INT, 0);

    if (coordinate_system == Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX) {
        Ichigo::Internal::gl.glUniform2f(screen_dimension_uniform, (f32) Ichigo::Camera::screen_tile_dimensions.x, (f32) Ichigo::Camera::screen_tile_dimensions.y);
    }

    // Thanks for the memory :)
    END_TEMP_MEMORY(Ichigo::game_state.transient_storage_arena, temp_ptr);
}

// If the entity does not provide its own render procedure (likely), this one is run.
void default_entity_render_proc(Ichigo::Entity *entity) {
#ifdef ICHIGO_DEBUG
    if (entity->sprite.width <= 0.0f || entity->sprite.height <= 0.0f) goto skip_sprite_draw;
#else
    if (entity->sprite.width <= 0.0f || entity->sprite.height <= 0.0f) return;
#endif

#ifdef ICHIGO_DEBUG
    // FIXME: This does not advance animation frames. Games that depend on the animation ending to change state break with this option on. Just throw it in the same place as the INVISIBLE flag.
    if (!DEBUG_hide_entity_sprites) {
        render_sprite_and_advance_animation(
            Ichigo::CoordinateSystem::WORLD,
            entity->col.pos,
            &entity->sprite,
            FLAG_IS_SET(entity->flags, Ichigo::EF_FLIP_H),
            !FLAG_IS_SET(entity->flags, Ichigo::EF_INVISIBLE)
        );
    }
#else
        render_sprite_and_advance_animation(
            Ichigo::CoordinateSystem::WORLD,
            entity->col.pos,
            &entity->sprite,
            FLAG_IS_SET(entity->flags, Ichigo::EF_FLIP_H),
            !FLAG_IS_SET(entity->flags, Ichigo::EF_INVISIBLE)
        );
#endif

#ifdef ICHIGO_DEBUG
skip_sprite_draw:
    if (DEBUG_draw_colliders) {
        Ichigo::world_render_solid_colour_rect(entity->col, {1.0f, 0.2f, 0.2f, 0.6f});

        Ichigo::TextStyle style;
        style.scale     = 0.5f;
        style.alignment = Ichigo::TextAlignment::LEFT;
        style.colour    = {1.0f, 0.0f, 1.0f, 0.8f};

        usize name_len = std::strlen(entity->name);
        f32 text_width = get_text_width(entity->name, name_len, style);
        Vec2<f32> pos = entity->col.pos + Vec2<f32>{0.0f, -0.05f};
        Ichigo::world_render_solid_colour_rect({pos + Vec2<f32>{0.0f, -0.15f}, text_width, 0.2f}, {0.0f, 0.0f, 0.0f, 0.8f});
        render_text(pos, entity->name, name_len, Ichigo::CoordinateSystem::WORLD, style);
    }
#endif
}

void Ichigo::set_tilemap(Tilemap *tilemap) {
    assert(tilemap->width > 0 && tilemap->height > 0 && tilemap->width * tilemap->height < ICHIGO_MAX_TILEMAP_SIZE && tilemap->tile_info_count < ICHIGO_MAX_UNIQUE_TILES);
    Internal::current_tilemap.width           = tilemap->width;
    Internal::current_tilemap.height          = tilemap->height;
    Internal::current_tilemap.tile_info_count = tilemap->tile_info_count;
    Internal::current_tilemap.sheet           = tilemap->sheet;

    std::memset(Internal::current_tilemap.tiles, 0, tilemap->width * tilemap->height * sizeof(TileID));
    std::memset(Internal::current_tilemap.tile_info, 0, tilemap->tile_info_count * sizeof(TileInfo));
    std::memcpy(Internal::current_tilemap.tiles, tilemap->tiles, tilemap->width * tilemap->height * sizeof(TileID));
    std::memcpy(Internal::current_tilemap.tile_info, tilemap->tile_info, tilemap->tile_info_count * sizeof(TileInfo));
}

void Ichigo::set_tilemap(u8 *ichigo_tilemap_memory, Ichigo::SpriteSheet tileset_sheet) {
    if (!ichigo_tilemap_memory) {
        Internal::current_tilemap.width           = 0;
        Internal::current_tilemap.height          = 0;
        Internal::current_tilemap.tile_info_count = 0;

        return;
    }

    Tilemap tilemap;
    usize cursor = 0;
    // Version number
    assert((u16) ichigo_tilemap_memory[cursor] == 2);
    cursor += sizeof(u16);

    tilemap.tile_info_count = *((u32 *) &ichigo_tilemap_memory[cursor]);
    cursor += sizeof(u32);

    tilemap.tile_info = (TileInfo *) &ichigo_tilemap_memory[cursor];
    cursor += sizeof(TileInfo) * tilemap.tile_info_count;

    tilemap.width = *((u32 *) &ichigo_tilemap_memory[cursor]);
    cursor += sizeof(u32);

    tilemap.height = *((u32 *) &ichigo_tilemap_memory[cursor]);
    cursor += sizeof(u32);

    tilemap.tiles           = (TileID *) &ichigo_tilemap_memory[cursor];
    tilemap.sheet           = tileset_sheet;

    set_tilemap(&tilemap);
}


// void Ichigo::set_tilemap(u8 *ichigo_tilemap_memory, TileInfo *tile_info, u32 tile_info_count, Ichigo::SpriteSheet tileset_sheet) {
//     Tilemap tilemap;
//     usize cursor = 0;
//     // Version number
//     assert((u16) ichigo_tilemap_memory[cursor] == 1);
//     cursor += sizeof(u16);

//     tilemap.width = (u32) ichigo_tilemap_memory[cursor];
//     cursor += sizeof(u32);

//     tilemap.height = (u32) ichigo_tilemap_memory[cursor];
//     cursor += sizeof(u32);

//     tilemap.tiles           = (TileID *) &ichigo_tilemap_memory[cursor];
//     tilemap.tile_info       = tile_info;
//     tilemap.tile_info_count = tile_info_count;
//     tilemap.sheet           = tileset_sheet;

//     set_tilemap(&tilemap);
// }

Ichigo::TileID Ichigo::tile_at(Vec2<i32> tile_coord) {
    if (!Internal::current_tilemap.tiles || tile_coord.x >= (i32) Internal::current_tilemap.width || tile_coord.x < 0 || tile_coord.y >= (i32) Internal::current_tilemap.height || tile_coord.y < 0)
        return ICHIGO_INVALID_TILE;

    return Internal::current_tilemap.tiles[tile_coord.y * Internal::current_tilemap.width + tile_coord.x];
}

Ichigo::TileInfo Ichigo::get_tile_info(TileID tile_id) {
    assert(tile_id < Internal::current_tilemap.tile_info_count);
    return Internal::current_tilemap.tile_info[tile_id];
}

// Build the vertex and index buffers for drawing one tile in the tilemap.
static void build_tile_draw_data(Vec2<i32> tile_pos, Bana::BufferBuilder<TexturedVertex> &vertices, Bana::BufferBuilder<u32> &indices) {
    Ichigo::TileID tile = Ichigo::tile_at(tile_pos);

    if (tile == ICHIGO_INVALID_TILE) {
        // TODO: This is still done per-tile. But this codepath will only ever get lit up in the level editor. So, who cares, right?
        // if (program_mode == Ichigo::Internal::EDITOR) {
        //     Ichigo::world_render_textured_rect({{(f32) tile_pos.x, (f32) tile_pos.y}, 1.0f, 1.0f}, invalid_tile_texture_id);
        // }

        return;
    }

    const Ichigo::TileInfo &tile_info = Ichigo::Internal::current_tilemap.tile_info[tile];

    if (tile_info.cell < 0) {
        return;
    }

    const Ichigo::Texture &texture = Ichigo::Internal::textures[Ichigo::Internal::current_tilemap.sheet.texture];

    // Same old same old.
    u32 cells_per_row = texture.width  / Ichigo::Internal::current_tilemap.sheet.cell_width;
    u32 row_of_cell   = tile_info.cell / cells_per_row;
    u32 col_of_cell   = tile_info.cell % cells_per_row;
    u32 cell_x_px     = col_of_cell    * Ichigo::Internal::current_tilemap.sheet.cell_width;
    u32 cell_y_px     = row_of_cell    * Ichigo::Internal::current_tilemap.sheet.cell_height;

    f32 u0 = (f32) cell_x_px / (f32) texture.width;
    f32 v0 = (f32) (texture.height - cell_y_px) / (f32) texture.height;
    f32 u1 = u0 + ((f32) Ichigo::Internal::current_tilemap.sheet.cell_width  / (f32) texture.width);
    f32 v1 = v0 - ((f32) Ichigo::Internal::current_tilemap.sheet.cell_height / (f32) texture.height);

    u32 in[] = {
        (u32) vertices.size,     (u32) vertices.size + 1, (u32) vertices.size + 2,
        (u32) vertices.size + 1, (u32) vertices.size + 2, (u32) vertices.size + 3
    };

    indices.append(in, ARRAY_LEN(in));

    Vec2<f32> draw_pos = { (f32) tile_pos.x, (f32) tile_pos.y };
    TexturedVertex vtx[] = {
        {{draw_pos.x, draw_pos.y, 0.0f},         {u0, v0}},  // top left
        {{draw_pos.x + 1, draw_pos.y, 0.0f},     {u1, v0}}, // top right
        {{draw_pos.x, draw_pos.y + 1, 0.0f},     {u0, v1}}, // bottom left
        {{draw_pos.x + 1, draw_pos.y + 1, 0.0f}, {u1, v1}}, // bottom right
    };

    vertices.append(vtx, ARRAY_LEN(vtx));
}

void Ichigo::push_draw_command(DrawCommand draw_command) {
    assert(Ichigo::game_state.this_frame_data.draw_command_count < MAX_DRAW_COMMANDS);
    Ichigo::game_state.this_frame_data.draw_commands[Ichigo::game_state.this_frame_data.draw_command_count] = draw_command;
    ++Ichigo::game_state.this_frame_data.draw_command_count;
}

void Ichigo::render_rect_deferred(CoordinateSystem coordinate_system, Rect<f32> rect, Vec4<f32> colour, Mat4<f32> transform) {
    Ichigo::DrawCommand c = {
        .type              = SOLID_COLOUR_RECT,
        .coordinate_system = coordinate_system,
        .transform         = transform,
        .rect              = rect,
        .colour            = colour
    };

    Ichigo::push_draw_command(c);
}

void Ichigo::render_rect_deferred(CoordinateSystem coordinate_system, Rect<f32> rect, TextureID texture_id, Mat4<f32> transform, Vec4<f32> tint) {
    Ichigo::DrawCommand c = {
        .type              = TEXTURED_RECT,
        .coordinate_system = coordinate_system,
        .transform         = transform,
        .texture_rect      = rect,
        .texture_id        = texture_id,
        .texture_tint      = tint
    };

    Ichigo::push_draw_command(c);
}

void Ichigo::render_sprite_and_advance_animation_deferred(CoordinateSystem coordinate_system, Vec2<f32> pos, Sprite *sprite, bool flip_h, bool actually_render, Mat4<f32> transform, Vec4<f32> tint) {
    Ichigo::DrawCommand c = {
        .type                   = SPRITE,
        .coordinate_system      = coordinate_system,
        .transform              = transform,
        .sprite                 = sprite,
        .sprite_tint            = tint,
        .sprite_pos             = pos,
        .sprite_flip_h          = flip_h,
        .actually_render_sprite = actually_render
    };

    Ichigo::push_draw_command(c);
}


static InfoLogMessage *info_log;
static char *info_log_string_data;
static i32 info_log_top = 0;
void Ichigo::show_info(const char *str, u32 length) {
    assert(length < INFO_LOG_MAX_BYTE_LENGTH);
    char *data = &info_log_string_data[info_log_top * INFO_LOG_MAX_BYTE_LENGTH];
    std::memcpy(data, str, length);

    InfoLogMessage msg = {};
    msg.data           = data;
    msg.length         = length;
    msg.t_remaining    = 3.0f; // TODO: Configurable?

    info_log[info_log_top] = msg;
    info_log_top = (info_log_top + 1) % INFO_LOG_MAX_MESSAGES;
}

// Convenience function for showing null terminated "C" strings.
void Ichigo::show_info(const char *cstr) {
    show_info(cstr, std::strlen(cstr));
}

static void draw_info_log() {
    for (i32 i = DEC_POSITIVE_OR(info_log_top, INFO_LOG_MAX_MESSAGES - 1), j = 0; j < INFO_LOG_MAX_MESSAGES; i = DEC_POSITIVE_OR(i, INFO_LOG_MAX_MESSAGES - 1), ++j) {
        InfoLogMessage &msg = info_log[i];

        if (msg.t_remaining <= 0.0f) {
            continue;
        }

        msg.t_remaining -= Ichigo::Internal::dt;

        f32 alpha = msg.t_remaining < 1.0f ? msg.t_remaining : 1.0f;

        Ichigo::TextStyle style;
        style.scale     = 1.2f;
        style.alignment = Ichigo::TextAlignment::CENTER;
        style.colour    = {0.0f, 0.0f, 0.0f, alpha};

        Vec2<f32> pos = {(f32) SCREEN_ASPECT_FIX_WIDTH / 2.0f, 4.0f - (j * 0.5f)};
        render_text(pos, msg.data, msg.length, Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX, style);

        style.colour = {0.9f, 0.9f, 0.9f, alpha};
        render_text(pos + Vec2<f32>{0.007, 0.007}, msg.data, msg.length, Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX, style);
    }
}

static f32 calculate_background_start_position(f32 camera_offset, f32 texture_width_in_metres, f32 scroll_speed) {
    // The start of the infinite looping background (in metres) is defined by:
    // begin_x = n*texture_width + offset * scroll_speed
    // And, let n be the "number of backgrounds already scrolled passed". For example, once the camera has scrolled passed one whole background,
    // n would be 1. You can think of this number telling us "what index we need to start at".
    // Imagine there are an infinite number of these backgrounds stuck together. We only ever need to draw 2 of them to cover the whole screen,
    // so the question is which 2 do we draw? Using the value for n, we know how many of these background "indices" to skip over, so we get
    // the correct x position in world space to draw the first background tile. Then, we just draw the second one after that.
    // The calculation for n is done by calculating how far the camera has to scroll to get passed one background tile.
    // This is the (texture_width_in_metres / scroll_speed) part. If the background scrolls at half the speed of the camera (0.5f)
    // and the texture is 16.0f metres wide, then the camera must scroll 16 * (1/0.5) = 16 * 2 = 32 metres.
    return ((u32) ((camera_offset / safe_ratio_1(texture_width_in_metres, scroll_speed)))) * texture_width_in_metres + camera_offset * (1.0f - scroll_speed);
}

// Render one frame.
static void frame_render() {
#ifdef ICHIGO_DEBUG
    ImGui::Render();
#endif

    if (Ichigo::Internal::window_width == 0 || Ichigo::Internal::window_height == 0) {
        return;
    }

    Ichigo::Internal::gl.glViewport(Ichigo::Internal::viewport_origin.x, Ichigo::Internal::viewport_origin.y, Ichigo::Internal::viewport_width, Ichigo::Internal::viewport_height);
    Ichigo::Internal::gl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    Ichigo::Internal::gl.glClear(GL_COLOR_BUFFER_BIT);

    // Reset uniforms
    // TODO: Use global uniforms for camera transform?
    Ichigo::Internal::gl.glUseProgram(texture_shader_program);

    i32 camera_uniform  = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "camera_transform");
    Ichigo::Internal::gl.glUniformMatrix4fv(camera_uniform, 1, GL_TRUE, (GLfloat *) &Ichigo::Camera::transform);

    i32 screen_dimensions_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "screen_tile_dimensions");
    Ichigo::Internal::gl.glUniform2f(screen_dimensions_uniform, (f32) Ichigo::Camera::screen_tile_dimensions.x, (f32) Ichigo::Camera::screen_tile_dimensions.y);

    Ichigo::Internal::gl.glUseProgram(text_shader_program);

    camera_uniform  = Ichigo::Internal::gl.glGetUniformLocation(text_shader_program, "camera_transform");
    Ichigo::Internal::gl.glUniformMatrix4fv(camera_uniform, 1, GL_TRUE, (GLfloat *) &Ichigo::Camera::transform);

    screen_dimensions_uniform = Ichigo::Internal::gl.glGetUniformLocation(text_shader_program, "screen_tile_dimensions");
    Ichigo::Internal::gl.glUniform2f(screen_dimensions_uniform, (f32) Ichigo::Camera::screen_tile_dimensions.x, (f32) Ichigo::Camera::screen_tile_dimensions.y);

    Ichigo::Internal::gl.glUseProgram(solid_colour_shader_program);

    camera_uniform = Ichigo::Internal::gl.glGetUniformLocation(solid_colour_shader_program, "camera_transform");
    Ichigo::Internal::gl.glUniformMatrix4fv(camera_uniform, 1, GL_TRUE, (GLfloat *) &Ichigo::Camera::transform);

    screen_dimensions_uniform = Ichigo::Internal::gl.glGetUniformLocation(solid_colour_shader_program, "screen_tile_dimensions");
    Ichigo::Internal::gl.glUniform2f(screen_dimensions_uniform, (f32) Ichigo::Camera::screen_tile_dimensions.x, (f32) Ichigo::Camera::screen_tile_dimensions.y);

    // == Background ==
    // Background colour
    screen_render_solid_colour_rect({{0.0f, 0.0f}, 1.0f, 1.0f}, Ichigo::game_state.background_colour);

    for (u32 i = 0; i < ICHIGO_MAX_BACKGROUNDS; ++i) {
        if (Ichigo::game_state.background_layers[i].texture_id != 0) {
            Ichigo::Background &bg = Ichigo::game_state.background_layers[i];
            Ichigo::Texture &bg_texture = Ichigo::Internal::textures[Ichigo::game_state.background_layers[i].texture_id];

            // TODO: BG_REPEAT_Y
            // FIXME: start_position is kind of a lie.
            if (FLAG_IS_SET(bg.flags, Ichigo::BG_REPEAT_X)) {
                auto camera_offset                       = -1.0f * get_translation2d(Ichigo::Camera::transform);
                f32 width_metres                         = pixels_to_metres(bg_texture.width);
                f32 beginning_of_first_texture_in_screen = bg.start_position.x + calculate_background_start_position(camera_offset.x, width_metres, bg.scroll_speed.x);
                f32 y_of_first_texture                   = bg.start_position.y + camera_offset.y * (1.0f - bg.scroll_speed.y);
                f32 end_of_first_texture_in_screen       = beginning_of_first_texture_in_screen + width_metres;

                Ichigo::world_render_textured_rect(
                    {
                        {beginning_of_first_texture_in_screen, bg.start_position.y + y_of_first_texture},
                        width_metres, pixels_to_metres(bg_texture.height)
                    },
                    bg.texture_id
                );
                Ichigo::world_render_textured_rect(
                    {
                        {end_of_first_texture_in_screen, bg.start_position.y + y_of_first_texture},
                        width_metres, pixels_to_metres(bg_texture.height)
                    },
                    bg.texture_id
                );
            } else {
                Ichigo::world_render_textured_rect(
                    {
                        {bg.start_position.x, bg.start_position.y},
                        pixels_to_metres(bg_texture.width), pixels_to_metres(bg_texture.height)
                    },
                    bg.texture_id
                );
            }
        }
    }
    // == End background ==

    // == Tilemap ==
    i64             row_count          = (i32) std::ceilf(Ichigo::Camera::screen_tile_dimensions.y) + 1;
    i64             col_count          = (i32) std::ceilf(Ichigo::Camera::screen_tile_dimensions.x) + 1;
    i64             vertex_buffer_size = (row_count * col_count) * 4;
    i64             index_buffer_size  = (row_count * col_count) * 6;
    TexturedVertex *vertex_buffer      = PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, TexturedVertex, vertex_buffer_size);
    u32            *index_buffer       = PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, u32, index_buffer_size);

#ifdef ICHIGO_DEBUG
    u32 *overrun_flag = PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, u32, 1);
    *overrun_flag = 0;
#endif

    Bana::BufferBuilder<TexturedVertex> vertices(vertex_buffer, vertex_buffer_size);
    Bana::BufferBuilder<u32>            indices(index_buffer, index_buffer_size);

    i64 DEBUG_COUNT = 0;
    // Build the tilemap draw data.
    for (i32 row = (i32) std::floorf(Ichigo::Camera::offset.y); row < (i32) std::floorf(Ichigo::Camera::offset.y) + row_count; ++row) {
        for (i32 col = (i32) std::floorf(Ichigo::Camera::offset.x); col < (i32) std::floorf(Ichigo::Camera::offset.x) + col_count; ++col) {
            ++DEBUG_COUNT;
            build_tile_draw_data({col, row}, vertices, indices);
        }
    }

#ifdef ICHIGO_DEBUG
    assert(DEBUG_COUNT == row_count * col_count);
    if (*overrun_flag) {
        __builtin_debugtrap();
    }
#endif

    Ichigo::Internal::gl.glUseProgram(texture_shader_program);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, Ichigo::Internal::textures[Ichigo::Internal::current_tilemap.sheet.texture].id);

    Ichigo::Internal::gl.glBindBuffer(GL_ARRAY_BUFFER, draw_data_textured.vertex_buffer_id);
    Ichigo::Internal::gl.glBindVertexArray(draw_data_textured.vertex_array_id);

    Ichigo::Internal::gl.glBufferData(GL_ARRAY_BUFFER, vertices.size * sizeof(TexturedVertex), vertices.data, GL_STATIC_DRAW);
    Ichigo::Internal::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size * sizeof(u32), indices.data, GL_STATIC_DRAW);

    i32 texture_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "entity_texture");
    i32 object_uniform  = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "object_transform");
    i32 tint_uniform    = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "colour_tint");

    Ichigo::Internal::gl.glUniform4f(tint_uniform, 1.0f, 1.0f, 1.0f, 1.0f);
    Ichigo::Internal::gl.glUniform1i(texture_uniform, 0);
    Ichigo::Internal::gl.glUniformMatrix4fv(object_uniform, 1, GL_TRUE, (GLfloat *) &identity_mat4);

    Ichigo::Internal::gl.glDrawElements(GL_TRIANGLES, indices.size, GL_UNSIGNED_INT, 0);
    // == End tilemap ==

    // == Editor out-of-bounds boxes ==
    if (Ichigo::Camera::offset.x < 0.0f) {
        Ichigo::world_render_solid_colour_rect(
            {
                {Ichigo::Camera::offset.x, Ichigo::Camera::offset.y},
                std::fabsf(Ichigo::Camera::offset.x), Ichigo::Camera::screen_tile_dimensions.y
            },
            {0.0f, 0.0f, 0.0f, 0.5f}
        );
    }

    if (Ichigo::Camera::offset.y < 0.0f) {
        Ichigo::world_render_solid_colour_rect(
            {
                {Ichigo::Camera::offset.x < 0.0f ? 0.0f : Ichigo::Camera::offset.x, Ichigo::Camera::offset.y},
                Ichigo::Camera::screen_tile_dimensions.x, std::fabsf(Ichigo::Camera::offset.y)
            },
            {0.0f, 0.0f, 0.0f, 0.5f}
        );
    }

    if (Ichigo::Camera::offset.x + Ichigo::Camera::screen_tile_dimensions.x > Ichigo::Internal::current_tilemap.width) {
        Ichigo::world_render_solid_colour_rect(
            {
                {(f32) Ichigo::Internal::current_tilemap.width, Ichigo::Camera::offset.y},
                (Ichigo::Camera::offset.x + Ichigo::Camera::screen_tile_dimensions.x) - Ichigo::Internal::current_tilemap.width, Ichigo::Camera::screen_tile_dimensions.y
            },
            {0.0f, 0.0f, 0.0f, 0.5f}
        );
    }

    if (Ichigo::Camera::offset.y + Ichigo::Camera::screen_tile_dimensions.y > Ichigo::Internal::current_tilemap.height) {
        Ichigo::world_render_solid_colour_rect(
            {
                {Ichigo::Camera::offset.x, (f32) Ichigo::Internal::current_tilemap.height},
                Ichigo::Camera::screen_tile_dimensions.x, (Ichigo::Camera::offset.y + Ichigo::Camera::screen_tile_dimensions.y) - Ichigo::Internal::current_tilemap.height
            },
            {0.0f, 0.0f, 0.0f, 0.5f}
        );
    }
    // == End editor out-of-bounds boxes ==

    // == Entities ==
    Rect<f32> camera_rect = {
        Ichigo::Camera::offset,
        Ichigo::Camera::screen_tile_dimensions.x,
        Ichigo::Camera::screen_tile_dimensions.y,
    };

    for (u32 i = 0; i < Ichigo::Internal::entity_ids_in_draw_order.size; ++i) {
        Ichigo::Entity *entity = Ichigo::get_entity(Ichigo::Internal::entity_ids_in_draw_order[i]);
        if (entity && rectangles_intersect(camera_rect, entity->col)) {
            if (entity->render_proc) {
                entity->render_proc(entity);
            } else {
                default_entity_render_proc(entity);
            }
        }
    }

    // == End entities ==
    // == Draw commands ==
    // TODO: This is technically a "stack" I guess. Should we do this in stack order?
    for (u32 i = 0; i < Ichigo::game_state.this_frame_data.draw_command_count; ++i) {
        Ichigo::DrawCommand &cmd = Ichigo::game_state.this_frame_data.draw_commands[i];
        switch (cmd.type) {
            case Ichigo::DrawCommandType::SOLID_COLOUR_RECT: {
                switch (cmd.coordinate_system) {
                    case Ichigo::WORLD: {
                        Ichigo::world_render_solid_colour_rect(cmd.rect, cmd.colour, cmd.transform);
                    } break;

                    case Ichigo::CAMERA: {
                        Rect<f32> draw_rect = cmd.rect;
                        draw_rect.pos = cmd.rect.pos - get_translation2d(Ichigo::Camera::transform);
                        Ichigo::world_render_solid_colour_rect(draw_rect, cmd.colour, cmd.transform);
                    } break;

                    case Ichigo::SCREEN: {
                        screen_render_solid_colour_rect(cmd.rect, cmd.colour);
                    } break;

                    case Ichigo::SCREEN_ASPECT_FIX: {
                        i32 screen_dimension_uniform = Ichigo::Internal::gl.glGetUniformLocation(solid_colour_shader_program, "screen_tile_dimensions");

                        Ichigo::Internal::gl.glUniform2f(screen_dimension_uniform, (f32) SCREEN_ASPECT_FIX_WIDTH, (f32) SCREEN_ASPECT_FIX_HEIGHT);

                        Rect<f32> draw_rect = cmd.rect;
                        draw_rect.pos = cmd.rect.pos - get_translation2d(Ichigo::Camera::transform);
                        Ichigo::world_render_solid_colour_rect(draw_rect, cmd.colour, cmd.transform);

                        Ichigo::Internal::gl.glUniform2f(screen_dimension_uniform, (f32) Ichigo::Camera::screen_tile_dimensions.x, (f32) Ichigo::Camera::screen_tile_dimensions.y);
                    } break;

                    default: {
                        ICHIGO_ERROR("Invalid coordinate system!");
                    }
                }
            } break;

            case Ichigo::DrawCommandType::TEXTURED_RECT: {
                switch (cmd.coordinate_system) {
                    case Ichigo::WORLD: {
                        Ichigo::world_render_textured_rect(cmd.rect, cmd.texture_id, cmd.transform, cmd.texture_tint);
                    } break;

                    case Ichigo::CAMERA: {
                        Rect<f32> draw_rect = cmd.rect;
                        draw_rect.pos = cmd.rect.pos - get_translation2d(Ichigo::Camera::transform);
                        Ichigo::world_render_textured_rect(draw_rect, cmd.texture_id, cmd.transform, cmd.texture_tint);
                    } break;

                    case Ichigo::SCREEN: {
                        screen_render_textured_rect(cmd.rect, cmd.texture_id);
                    } break;

                    case Ichigo::SCREEN_ASPECT_FIX: {
                        i32 screen_dimension_uniform = Ichigo::Internal::gl.glGetUniformLocation(texture_shader_program, "screen_tile_dimensions");

                        Ichigo::Internal::gl.glUniform2f(screen_dimension_uniform, (f32) SCREEN_ASPECT_FIX_WIDTH, (f32) SCREEN_ASPECT_FIX_HEIGHT);

                        Rect<f32> draw_rect = cmd.rect;
                        draw_rect.pos = cmd.rect.pos - get_translation2d(Ichigo::Camera::transform);
                        Ichigo::world_render_textured_rect(draw_rect, cmd.texture_id, cmd.transform, cmd.texture_tint);

                        Ichigo::Internal::gl.glUniform2f(screen_dimension_uniform, (f32) Ichigo::Camera::screen_tile_dimensions.x, (f32) Ichigo::Camera::screen_tile_dimensions.y);
                    } break;

                    default: {
                        ICHIGO_ERROR("Invalid coordinate system!");
                    }
                }
            } break;

            case Ichigo::DrawCommandType::TEXT: {
                render_text(cmd.string_pos, cmd.string, cmd.string_length, cmd.coordinate_system, cmd.text_style);
            } break;

            case Ichigo::DrawCommandType::SPRITE: {
                render_sprite_and_advance_animation(cmd.coordinate_system, cmd.sprite_pos, cmd.sprite, cmd.sprite_flip_h, cmd.actually_render_sprite, cmd.transform, cmd.sprite_tint);
            } break;

            default: {
                ICHIGO_ERROR("Invalid draw command type: %u", cmd.type);
            }
        }
    }
    // == End draw commands ==

    draw_info_log();

#ifdef ICHIGO_DEBUG
    if (program_mode == Ichigo::Internal::ProgramMode::EDITOR) {
        Ichigo::Editor::render();
    }

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

    Ichigo::Internal::gl.glFinish();
}

// Simulation and logic for one frame.
void Ichigo::Internal::do_frame() {
    if (Ichigo::Internal::dt > 0.1)
        Ichigo::Internal::dt = 0.1;

    // static bool do_wireframe = 0;
    RESET_ARENA(Ichigo::game_state.transient_storage_arena);

    Ichigo::game_state.this_frame_data.draw_commands      = PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, DrawCommand, MAX_DRAW_COMMANDS);
    Ichigo::game_state.this_frame_data.draw_command_count = 0;

    Ichigo::Game::frame_begin();

    if (Ichigo::Internal::window_height != last_window_height || Ichigo::Internal::window_width != last_window_width) {
        last_window_height                 = Ichigo::Internal::window_height;
        last_window_width                  = Ichigo::Internal::window_width;

        // Always resize the viewport to a 16:9 resolution.
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

    if (dpi_scale != last_dpi_scale) {
        ICHIGO_INFO("scaling to scale=%f", dpi_scale);
#ifdef ICHIGO_DEBUG
        auto io = ImGui::GetIO();
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        io.Fonts->Clear();
        io.Fonts->AddFontFromMemoryTTF((void *) noto_font, noto_font_len, static_cast<i32>(18 * dpi_scale), &font_config, io.Fonts->GetGlyphRangesJapanese());
        io.Fonts->Build();
        ImGui_ImplOpenGL3_CreateFontsTexture();
        // Scale all Dear ImGui sizes based on the inital style
        std::memcpy(&ImGui::GetStyle(), &initial_style, sizeof(initial_style));
        ImGui::GetStyle().ScaleAllSizes(dpi_scale);
#endif
        last_dpi_scale = dpi_scale;
    }

#ifdef ICHIGO_DEBUG
    // Toggle the debug menu when F3 is pressed.
    if (Ichigo::Internal::keyboard_state[Ichigo::IK_F3].down_this_frame) {
        show_debug_menu = !show_debug_menu;
    }

    // Hard reset the level when you press Ctrl + R
    if (Ichigo::Internal::keyboard_state[Ichigo::IK_LEFT_CONTROL].down && Ichigo::Internal::keyboard_state[Ichigo::IK_R].down_this_frame) {
        Ichigo::Game::hard_reset_level();
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    if (program_mode == Ichigo::Internal::ProgramMode::EDITOR) {
        Ichigo::Editor::render_ui();
    }

    // Render the debug menu.
    if (show_debug_menu) {
        ImGui::SetNextWindowPos({0.0f, 0.0f});
        ImGui::SetNextWindowSize({Ichigo::Internal::window_width * 0.2f, (f32) Ichigo::Internal::window_height});
        ImGui::Begin("いちご！", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        // The engine of choice for strawberry loving idols!
        ImGui::Text("苺の力で...! がんばりまー");
        if (ImGui::Button("Show info log")) {
            static u32 fuck = 0;
            static char fuck_data[16] = {};
            std::snprintf(fuck_data, 15, "Hello%u", fuck);
            Ichigo::show_info(fuck_data, std::strlen(fuck_data));
            ++fuck;
        }

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
            ImGui::SeparatorText("Debug");
            ImGui::Checkbox("Draw colliders", &DEBUG_draw_colliders);
            ImGui::Checkbox("Hide sprites", &DEBUG_hide_entity_sprites);
            ImGui::SeparatorText("Entity List");
            for (u32 i = 0; i < Ichigo::Internal::entities.size; ++i) {
                Ichigo::Entity &entity = Ichigo::Internal::entities[i];

                if (entity.id.index == 0) {
                    if (ImGui::TreeNode((void *) (uptr) i, "Entity slot %u: (empty)", i)) {
                        ImGui::Text("No entity loaded in this slot.");
                        ImGui::TreePop();
                    }
                } else {
                    if (ImGui::TreeNodeEx((void *) (uptr) i, 0, "Entity slot %u: %s (%s)", i, entity.name, Internal::entity_id_as_string(entity.id))) {
                        ImGui::PushID(entity.id.index);
                        if (ImGui::Button("Follow"))
                            Ichigo::Camera::follow(entity.id);
                        ImGui::PopID();

                        ImGui::Text("pos=%f,%f", entity.col.pos.x, entity.col.pos.y);
                        ImGui::Text("acceleration=%f,%f", entity.acceleration.x, entity.acceleration.y);
                        ImGui::Text("velocity=%f,%f", entity.velocity.x, entity.velocity.y);
                        ImGui::Text("on ground?=%d", FLAG_IS_SET(entity.flags, Ichigo::EntityFlag::EF_ON_GROUND));
                        ImGui::Text("left_standing_tile=%d,%d", entity.left_standing_tile.x, entity.left_standing_tile.y);
                        ImGui::Text("right_standing_tile=%d,%d", entity.right_standing_tile.x, entity.right_standing_tile.y);
                        ImGui::Text("animation_tag=%u", entity.sprite.animation.tag);
                        ImGui::Text("current_anim_frame=%u", entity.sprite.current_animation_frame);
                        ImGui::Text("anim_t=%f", entity.sprite.elapsed_animation_frame_time);
                        ImGui::Text("anim_t_per_frame=%f", entity.sprite.animation.seconds_per_frame);
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
                Ichigo::Mixer::play_audio_oneshot(test_sound_id, next_test_sound_volume, next_test_sound_volume_l, next_test_sound_volume_r);

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
                Mixer::PlayingAudio &pa = Mixer::playing_audio[i];
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

    // Get the game to update if we are in game mode. Otherwise, the editor takes over.
    if (program_mode == Ichigo::Internal::ProgramMode::GAME) {
        for (u32 i = 1; i < Ichigo::Internal::entities.size; ++i) {
            Ichigo::Entity &entity = Ichigo::Internal::entities[i];
            if (entity.id.index != 0 && entity.update_proc) {
                entity.update_proc(&entity);
            }
        }

        // TODO: Let the game update first? Not sure? Maybe they want to change something about an entity before it gets updated.
        //       Also we might just consider making the game call an Ichigo::update() function so that the game can control when everything updates.
        //       But also we have frame begin and frame end...?
        Ichigo::Game::update_and_render();

        // DEBUG
        // Open the editor when you press F1.
        if (Ichigo::Internal::keyboard_state[IK_F1].down_this_frame) {
            Ichigo::Camera::mode = Ichigo::Camera::Mode::MANUAL;
            Ichigo::Camera::manual_focus_point = Ichigo::Camera::offset;

            Ichigo::Editor::before_open();
            program_mode = Ichigo::Internal::ProgramMode::EDITOR;
        }
    } else if (program_mode == Ichigo::Internal::ProgramMode::EDITOR) {
        Ichigo::Editor::update();

        // DEBUG
        // Close the editor when you press F1.
        if (Ichigo::Internal::keyboard_state[IK_F1].down_this_frame) {
            Ichigo::Editor::before_close();
            Ichigo::Camera::mode = Ichigo::Camera::Mode::FOLLOW;
            program_mode = Ichigo::Internal::ProgramMode::GAME;
        }
    }
#else // NOTE: We completely skip the "program_mode" logic when we are not in debug mode. This is just a copy of the code above that runs in "game mode"
    for (u32 i = 1; i < Ichigo::Internal::entities.size; ++i) {
        Ichigo::Entity &entity = Ichigo::Internal::entities[i];
        if (entity.id.index != 0 && entity.update_proc) {
            entity.update_proc(&entity);
        }
    }

    // TODO: Let the game update first? Not sure? Maybe they want to change something about an entity before it gets updated.
    //       Also we might just consider making the game call an Ichigo::update() function so that the game can control when everything updates.
    //       But also we have frame begin and frame end...?
    Ichigo::Game::update_and_render();
#endif

    Ichigo::Camera::update();

    if (Ichigo::Internal::window_height != 0 && Ichigo::Internal::window_width != 0) {
        frame_render();
    }

    Ichigo::conduct_end_of_frame_executions();
    Ichigo::Game::frame_end();
}

// Compile a GLSL shader.
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

// Link an OpenGL shader program.
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
    BEGIN_TIMED_BLOCK(engine_init);
    identity_mat4 = m4identity();

    // Load images flipped. OpenGL coordinates are upside down :)
    stbi_set_flip_vertically_on_load(true);

    // TODO: Platform specific allocations? VirtualAlloc() on win32
    Ichigo::game_state.transient_storage_arena.pointer  = 0;
    Ichigo::game_state.transient_storage_arena.capacity = MEGABYTES(64);
    Ichigo::game_state.transient_storage_arena.data     = (u8 *) malloc(Ichigo::game_state.transient_storage_arena.capacity);

    Ichigo::game_state.permanent_storage_arena.pointer  = 0;
    Ichigo::game_state.permanent_storage_arena.capacity = MEGABYTES(256);
    Ichigo::game_state.permanent_storage_arena.data     = (u8 *) malloc(Ichigo::game_state.permanent_storage_arena.capacity);

    Ichigo::Internal::current_tilemap.tiles     = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, TileID, ICHIGO_MAX_TILEMAP_SIZE);
    Ichigo::Internal::current_tilemap.tile_info = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, TileInfo, ICHIGO_MAX_UNIQUE_TILES);
    info_log_string_data                        = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, char, INFO_LOG_MAX_BYTE_LENGTH * INFO_LOG_MAX_MESSAGES);
    info_log                                    = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, InfoLogMessage, INFO_LOG_MAX_MESSAGES);

    std::memset(info_log, 0, INFO_LOG_MAX_MESSAGES * sizeof(InfoLogMessage));

#ifdef ICHIGO_DEBUG
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
#endif

    kanji_hash_map.data            = kanji_codepoints;
    kanji_hash_map.codepoint_count = kanji_codepoint_count;

    BEGIN_TIMED_BLOCK(font_atlas_init);

    // NOTE: The font bitmap is in temporary memory since we don't care about it after we upload it to the GPU.
    u8 *font_bitmap               = PUSH_ARRAY(Ichigo::game_state.transient_storage_arena, u8, ICHIGO_FONT_ATLAS_WIDTH * ICHIGO_FONT_ATLAS_HEIGHT);
    printable_ascii_pack_data     = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, stbtt_packedchar, character_ranges[0].length);
    cjk_pack_data                 = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, stbtt_packedchar, character_ranges[1].length);
    kanji_pack_data               = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, stbtt_packedchar, kanji_codepoint_count);
    half_and_full_width_pack_data = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, stbtt_packedchar, character_ranges[3].length);

    stbtt_pack_context spc = {};
    stbtt_PackBegin(&spc, font_bitmap, ICHIGO_FONT_ATLAS_WIDTH, ICHIGO_FONT_ATLAS_HEIGHT, 0, 1, nullptr);

    stbtt_pack_range ranges[4] = {};

    // Printable ASCII
    ranges[0].first_unicode_codepoint_in_range = character_ranges[0].first_codepoint;
    ranges[0].num_chars                        = character_ranges[0].length;
    ranges[0].chardata_for_range               = printable_ascii_pack_data;
    ranges[0].font_size                        = ICHIGO_FONT_PIXEL_HEIGHT;

    // CJK punctuation, hiragana, katakana
    ranges[1].first_unicode_codepoint_in_range = character_ranges[1].first_codepoint;
    ranges[1].num_chars                        = character_ranges[1].length;
    ranges[1].chardata_for_range               = cjk_pack_data;
    ranges[1].font_size                        = ICHIGO_FONT_PIXEL_HEIGHT;

    // Joyo and Jinmeiyo kanji
    ranges[2].array_of_unicode_codepoints      = (i32 *) kanji_codepoints;
    ranges[2].num_chars                        = kanji_codepoint_count;
    ranges[2].chardata_for_range               = kanji_pack_data;
    ranges[2].font_size                        = ICHIGO_FONT_PIXEL_HEIGHT;

    // Halfwidth and fullwidth forms
    ranges[3].first_unicode_codepoint_in_range = character_ranges[3].first_codepoint;
    ranges[3].num_chars                        = character_ranges[3].length;
    ranges[3].chardata_for_range               = half_and_full_width_pack_data;
    ranges[3].font_size                        = ICHIGO_FONT_PIXEL_HEIGHT;

    stbtt_PackSetOversampling(&spc, 2, 2);
    stbtt_PackFontRanges(&spc, noto_font, 0, ranges, ARRAY_LEN(ranges));
    stbtt_PackEnd(&spc);

    Ichigo::Internal::gl.glGenTextures(1, &font_atlas_texture_id);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, font_atlas_texture_id);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    Ichigo::Internal::gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ICHIGO_FONT_ATLAS_WIDTH, ICHIGO_FONT_ATLAS_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, font_bitmap);

    END_TIMED_BLOCK(font_atlas_init);

    ICHIGO_INFO("GL_VERSION=%s", Ichigo::Internal::gl.glGetString(GL_VERSION));

    Ichigo::Internal::gl.glEnable(GL_BLEND);
    Ichigo::Internal::gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint screenspace_vertex_shader    = Ichigo::Internal::gl.glCreateShader(GL_VERTEX_SHADER);
    GLuint worldspace_vertex_shader     = Ichigo::Internal::gl.glCreateShader(GL_VERTEX_SHADER);
    GLuint texture_fragment_shader      = Ichigo::Internal::gl.glCreateShader(GL_FRAGMENT_SHADER);
    GLuint text_fragment_shader         = Ichigo::Internal::gl.glCreateShader(GL_FRAGMENT_SHADER);
    GLuint solid_colour_fragment_shader = Ichigo::Internal::gl.glCreateShader(GL_FRAGMENT_SHADER);

    compile_shader(screenspace_vertex_shader, (const GLchar *) screenspace_vertex_shader_source, screenspace_vertex_shader_source_len);
    compile_shader(worldspace_vertex_shader, (const GLchar *) vertex_shader_source, vertex_shader_source_len);
    compile_shader(texture_fragment_shader, (const GLchar *) fragment_shader_source, fragment_shader_source_len);
    compile_shader(text_fragment_shader, (const GLchar *) text_shader_source, text_shader_source_len);
    compile_shader(solid_colour_fragment_shader, (const GLchar *) solid_colour_fragment_shader_source, solid_colour_fragment_shader_source_len);

    texture_shader_program                = link_program(worldspace_vertex_shader, texture_fragment_shader);
    text_shader_program                   = link_program(worldspace_vertex_shader, text_fragment_shader);
    solid_colour_shader_program           = link_program(worldspace_vertex_shader, solid_colour_fragment_shader);
    screenspace_texture_program           = link_program(screenspace_vertex_shader, texture_fragment_shader);
    screenspace_text_shader_program       = link_program(screenspace_vertex_shader, text_fragment_shader);
    screenspace_solid_colour_rect_program = link_program(screenspace_vertex_shader, solid_colour_fragment_shader);

    Ichigo::Internal::gl.glDeleteShader(screenspace_vertex_shader);
    Ichigo::Internal::gl.glDeleteShader(worldspace_vertex_shader);
    Ichigo::Internal::gl.glDeleteShader(texture_fragment_shader);
    Ichigo::Internal::gl.glDeleteShader(text_fragment_shader);
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
    END_TIMED_BLOCK(engine_init);

    BEGIN_TIMED_BLOCK(game_init);
    Ichigo::Game::init();
    END_TIMED_BLOCK(game_init);

#ifdef ICHIGO_DEBUG
    test_sound_id = Ichigo::load_audio(test_sound, test_sound_len);
    show_info("You are running in debug mode. Press F3 for debug menu, F1 for editor.");
#endif
}

// This exists, maybe to save the game when you close it?
// But like, we don't really have to do anything else. The operating system is just going to clean up everything all at once when we exit.
void Ichigo::Internal::deinit() {}

void Ichigo::Internal::fill_sample_buffer(u8 *buffer, usize buffer_size, usize write_cursor_position_delta) {
    // assert(buffer_size % sizeof(i16) == 0);

    // Ensure that there is nothing in the mix buffer before we start mixing samples into it.
    std::memset(buffer, 0, buffer_size);
    Ichigo::Mixer::mix_into_buffer((AudioFrame2ChI16LE *) buffer, buffer_size, write_cursor_position_delta);
    // static u8 *DEBUG_ptr = music_buffer;
    // ICHIGO_INFO("write_cursor_position_delta=%llu", write_cursor_position_delta);
    // DEBUG_ptr += write_cursor_position_delta;
    // std::memcpy(buffer, DEBUG_ptr, buffer_size);
}
