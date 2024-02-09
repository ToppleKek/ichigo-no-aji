#pragma once
#include "util.hpp"
#include <string>
#include "opengl.hpp"

namespace Ichigo {
extern OpenGL gl;
extern bool must_rebuild_swapchain;
extern u32 window_width;
extern u32 window_height;
void init();
void do_frame(f32 dpi_scale);
void deinit();

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
}
