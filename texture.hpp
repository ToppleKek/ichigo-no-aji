#pragma once
#include "common.hpp"

namespace Ichigo {
using TextureID = u32;

// enum TextureID {
//     IT_NULL = 0,
//     IT_PLAYER,
//     IT_GRASS_TILE,
//     IT_ENUM_COUNT
// };

struct Texture {
    u32 width;
    u32 height;
    u32 id;
    u32 channel_count;
    u64 png_data_size;
    const u8 *png_data;
};

TextureID load_texture(const u8 *png_data, usize png_data_size);
}