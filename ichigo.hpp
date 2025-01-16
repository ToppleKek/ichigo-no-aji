/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Core module. Rendering, input, state, etc.

    Author:      Braeden Hong
    Last edited: 2024/11/30
*/

#pragma once

#include "asset.hpp"
#include "camera.hpp"
#include "entity.hpp"
#include "mixer.hpp"
#include "util.hpp"
#include "opengl.hpp"
#include "math.hpp"

#define MAX_TEXT_STRING_LENGTH 1024
#define ICHIGO_FONT_ATLAS_WIDTH  (1 << 12)
#define ICHIGO_FONT_ATLAS_HEIGHT (1 << 13)
#define ICHIGO_FONT_PIXEL_HEIGHT 76

#define ICHIGO_MAX_BACKGROUNDS 16
#define ICHIGO_MAX_TILEMAP_SIZE (4096 * 4096)
#define ICHIGO_MAX_UNIQUE_TILES 1024

// TODO: Replace all the places that we use a literal 0
#define ICHIGO_AIR_TILE 0

namespace Ichigo {
struct KeyState {
    bool down_this_frame;
    bool up_this_frame;
    bool down;
    bool up;
};

enum Keycode {
    IK_UNKNOWN,
    IK_BACKSPACE,
    IK_TAB,
    IK_ENTER,
    IK_LEFT_SHIFT,
    IK_LEFT_CONTROL,
    IK_RIGHT_SHIFT,
    IK_RIGHT_CONTROL,
    IK_ALT,
    IK_ESCAPE,
    IK_SPACE,
    IK_PAGE_UP,
    IK_PAGE_DOWN,
    IK_END,
    IK_HOME,
    IK_LEFT,
    IK_UP,
    IK_RIGHT,
    IK_DOWN,
    IK_PRINT_SCREEN,
    IK_INSERT,
    IK_DELETE,
    IK_F1,
    IK_F2,
    IK_F3,
    IK_F4,
    IK_F5,
    IK_F6,
    IK_F7,
    IK_F8,
    IK_F9,
    IK_F10,
    IK_F11,
    IK_F12,

    IK_0 = '0',
    IK_1,
    IK_2,
    IK_3,
    IK_4,
    IK_5,
    IK_6,
    IK_7,
    IK_8,
    IK_9,

    IK_A = 'A',
    IK_B,
    IK_C,
    IK_D,
    IK_E,
    IK_F,
    IK_G,
    IK_H,
    IK_I,
    IK_J,
    IK_K,
    IK_L,
    IK_M,
    IK_N,
    IK_O,
    IK_P,
    IK_Q,
    IK_R,
    IK_S,
    IK_T,
    IK_U,
    IK_V,
    IK_W,
    IK_X,
    IK_Y,
    IK_Z,
    IK_ENUM_COUNT
};

struct Gamepad {
    // Face buttons
    KeyState a;
    KeyState b;
    KeyState x;
    KeyState y;

    // D-pad
    KeyState up;
    KeyState down;
    KeyState left;
    KeyState right;

    // Shoulder buttons
    KeyState lb;
    KeyState rb;

    // Menu buttons
    KeyState start;
    KeyState select;

    // Triggers
    f32 lt;
    f32 rt;

    // Analog sticks
    KeyState stick_left_click;
    KeyState stick_right_click;
    Vec2<f32> stick_left;
    Vec2<f32> stick_right;

    bool connected;
};

struct Mouse {
    Vec2<i32> pos;
    f32 scroll_wheel_delta_this_frame;

    KeyState left_button;
    KeyState middle_button;
    KeyState right_button;
    KeyState button4;
    KeyState button5;
};

// "Draw commands" are how you render UI.
enum DrawCommandType {
    SOLID_COLOUR_RECT,
    TEXTURED_RECT,
    TEXT
};

enum CoordinateSystem {
    WORLD,    // The coordinates are specified in world units (metres).
    SCREEN,   // The coordinates are specified such that 0,0 is the top left of the screen, and 1,1 is the bottom right of the screen.
    CAMERA    // The coordinates are specified in world units (metres) relative to the camera.
};

enum TextAlignment {
    LEFT,
    CENTER,
    RIGHT
};

struct TextStyle {
    TextAlignment alignment;
    f32 scale;
    Vec4<f32> colour;
    f32 line_spacing; // TODO: This is in "unscaled font units". Make this a more reasonable unit.
};

struct DrawCommand {
    DrawCommandType type;
    CoordinateSystem coordinate_system;

    union {
        // SOLID_COLOUR_RECT
        struct {
            Rect<f32> rect;
            Vec4<f32> colour;
        };

        // TEXTURED_RECT
        struct {
            Rect<f32> texture_rect; // FIXME: stupid as fuck
            TextureID texture_id;
        };

        // TEXT
        struct {
            const char *string;
            // NOTE: "string_length" is in BYTES and not utf8 characters!
            usize string_length;
            Vec2<f32> string_pos;
            TextStyle text_style;
        };
    };
};

// Range of codepoints to include in font atlas construction.
struct CharRange {
    u32 first_codepoint;
    u32 length;
};

using BackgroundFlags = u32;

enum BackgroundFlag {
    BG_REPEAT_X = 1 << 0,
    BG_REPEAT_Y = 1 << 1, // TODO: Unimplemented.
};

struct Background {
    TextureID texture_id;
    BackgroundFlags flags;
    Vec2<f32> start_position;
    Vec2<f32> scroll_speed;
};

using TileFlags = u32;

enum TileFlag {
    TANGIBLE = 1 << 0, // Whether or not entities collide with this tile.
};

struct TileInfo {
    char name[8];
    i32 cell; // NOTE: A negative cell number means this tile does not render. Otherwise, it is the cell number in the tile set sprite sheet.
    TileFlags flags;
    f32 friction;
};

using TileID = u16;

struct Tilemap {
    TileID *tiles;
    u32 width;
    u32 height;
    TileInfo *tile_info;
    u32 tile_info_count;
    SpriteSheet sheet;
};

// Data used for rendering one frame. This stuff will all be allocated in transient storage.
struct FrameData {
    // Util::IchigoVector<DrawCommand> draw_commands;
    DrawCommand *draw_commands;
    usize draw_command_count;
};

// TODO: Perhaps this is not "game state". Rather, "engine state"?
struct GameState {
    Vec4<f32> background_colour;
    Background background_layers[ICHIGO_MAX_BACKGROUNDS];
    Bana::Arena transient_storage_arena;
    Bana::Arena permanent_storage_arena;
    FrameData this_frame_data;
};

extern GameState game_state;

// Set the tilemap directly.
void set_tilemap(Tilemap *tilemap);

// Load a tilemap from memory. Usually this is from a .ichigotm file.
void set_tilemap(u8 *ichigo_tilemap_memory, SpriteSheet tileset_sheet);

// Get the tile at the specified tile coordinate. Returns INVALID_TILE if the coordinate is out of range.
u16 tile_at(Vec2<i32> tile_coord);

// Push a draw command into the draw command buffer.
void push_draw_command(DrawCommand draw_command);
void render_text(Vec2<f32> pos, const char *str, usize length, Ichigo::CoordinateSystem coordinate_system, Ichigo::TextStyle style);
void world_render_solid_colour_rect(Rect<f32> rect, Vec4<f32> colour);
// Show a popup message in the info log.
void show_info(const char *str, u32 length);
void show_info(const char *cstr);

// Calculate the width of a string given the specified style.
f32 get_text_width(const char *str, usize length, Ichigo::TextStyle style);

// Implement these in your game to get started!
namespace Game {
void init();
void frame_begin();
void update_and_render();
void frame_end();
Bana::Array<Ichigo::EntityDescriptor> *level_entity_descriptors();
void set_level_entity_descriptors(Bana::FixedArray<Ichigo::EntityDescriptor> descriptors);
void hard_reset_level();
// Bana::FixedArray<Ichigo::EntityProperty> get_entity_type_information(u32 type_id);
// void set_entity_property_value(EntityID entity_id, EntityProperty property, void *data);
// void *get_entity_property_value(EntityID entity_id, EntityProperty property);
}

// Internal stuff. Generally okay to read from your game, maybe don't write to it unless you know what you're doing.
namespace Internal {
extern OpenGL gl;
extern bool must_rebuild_swapchain;
extern u32 window_width;
extern u32 window_height;
extern u32 viewport_width;
extern u32 viewport_height;
extern Vec2<u32> viewport_origin;
extern f32 target_frame_time;
extern f32 dpi_scale;
extern Tilemap current_tilemap;

// TODO: Move these out of internal?
extern f32 dt;
extern Ichigo::KeyState keyboard_state[Ichigo::Keycode::IK_ENUM_COUNT];
extern Gamepad gamepad;
extern Mouse mouse;

enum ProgramMode {
    GAME,
    EDITOR,
};

// The platform layer calls us through these.
void init();
void do_frame();
void deinit();
void fill_sample_buffer(u8 *buffer, usize buffer_size, usize write_cursor_position_delta);

// == Platform specific functions ==
struct PlatformFile;

PlatformFile *platform_open_file_write(const char *path);
void platform_write_entire_file_sync(const char *path, const u8 *data, usize data_size);

// Append to an open file. Generally works like fwrite() from the CRT.
void platform_append_file_sync(PlatformFile *file, const u8 *data, usize data_size);
void platform_close_file(PlatformFile *file);
// usize platform_read_entire_file_sync(const char *path, const u8 *data, usize data_size); // TODO: Implement on platform layers.

bool platform_file_exists(const char *path);
void platform_sleep(f64 t);
f64 platform_get_current_time();
void platform_pause_audio();
void platform_resume_audio();
}
}
