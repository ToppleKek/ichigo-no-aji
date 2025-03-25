/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Asset functions and structures.

    Author:      Braeden Hong
    Last edited: 2024/11/30
*/

#pragma once
#include "common.hpp"

namespace Ichigo {
using TextureID = u32;
using AudioID   = u32;

// enum TextureID {
//     IT_NULL = 0,
//     IT_PLAYER,
//     IT_GRASS_TILE,
//     IT_ENUM_COUNT
// };

struct Texture {
    u32 width;
    u32 height;
    u32 id; // NOTE: NOT a TextureID! This is the OpenGL id.
    u32 channel_count;
    u64 png_data_size;
    const u8 *png_data;
};

// A 2 channel signed 16-bit integer audio frame.
struct AudioFrame2ChI16LE {
    i16 l;
    i16 r;
};

struct Audio {
    usize size_in_bytes;
    AudioFrame2ChI16LE *frames;
};

TextureID load_texture(const u8 *png_data, usize png_data_size);
AudioID load_audio(const u8 *mp3_data, usize mp3_data_size);

namespace Internal {
extern Bana::Array<Ichigo::Texture> textures;
extern Bana::Array<Ichigo::Audio>   audio_assets;
}
}
