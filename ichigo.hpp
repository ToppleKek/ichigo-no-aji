#pragma once

#include "texture.hpp"
#include "camera.hpp"
#include "entity.hpp"
#include "util.hpp"
#include <string> // TODO: Remove this shit
#include "opengl.hpp"
#include "math.hpp"

namespace Ichigo {
struct KeyState {
    bool down_this_frame;
    bool up_this_frame;
    bool down;
    bool up;
};

enum Keycode {
    IK_UNKNOWN,
    IK_MOUSE_1,
    IK_MOUSE_2,
    IK_MOUSE_3,
    IK_MOUSE_4,
    IK_MOUSE_5,
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

struct GameState {
    Ichigo::EntityID player_entity_id;
};

extern GameState game_state;

void set_tilemap(u32 tilemap_width, u32 tilemap_height, u16 *tilemap, TextureID *tile_texture_map);
u16 tile_at(Vec2<u32> tile_coord);

namespace Game {
void init();
void frame_begin();
void update_and_render();
void frame_end();
}

namespace Internal {
extern OpenGL gl;
extern bool must_rebuild_swapchain;
extern u32 window_width;
extern u32 window_height;
extern f32 dpi_scale;
extern f32 dt;
extern Ichigo::KeyState keyboard_state[Ichigo::Keycode::IK_ENUM_COUNT];

extern u32 current_tilemap_width;
extern u32 current_tilemap_height;

void init();
void do_frame();
void deinit();
void fill_sample_buffer(u8 *buffer, usize buffer_size, usize write_cursor_position_delta);

/*
    Open a file with the specified mode.
    Parameter 'path': A string of the path to the file to open.
    Parameter 'mode': A string of the mode to open the file in (eg. rb, wb, a, etc.)
    Returns a standard C FILE struct.
*/
std::FILE *platform_open_file(const std::string &path, const std::string &mode);

/*
    Test if a file exists.
    Parameter 'path': A path to the file to test.
    Returns whether or not the file exists.
*/
bool platform_file_exists(const char *path);

/*
    Recurse a directory listing all files in the directory and all subdirectories.
    Parameter 'path': The path to the directory to recurse.
    Parameter 'extention_filter': An array of constant strings of file extensions. Only returns files that have these extensions.
    Parameter 'extension_filter_count': The size of the array.
*/
Util::IchigoVector<std::string> platform_recurse_directory(const std::string &path, const char **extension_filter, const u16 extension_filter_count);

void platform_sleep(f32 t);
f32 platform_get_current_time();
void platform_toggle_audio_playback();
}
}
