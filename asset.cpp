/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Asset loading functions.

    Author:      Braeden Hong
    Last edited: 2024/11/30
*/

// TODO: Ideally, we might want to introduce an "asset compilation" stage to the game build so that we don't have to decode PNGs and MP3s at runtime.

#include "asset.hpp"
#include "ichigo.hpp"

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static drmp3 mp3;

// TODO: @heap
Bana::Array<Ichigo::Texture> Ichigo::Internal::textures;
Bana::Array<Ichigo::Audio>   Ichigo::Internal::audio_assets;

// Decode a PNG and upload the resulting bitmap to the GPU.
Ichigo::TextureID Ichigo::load_texture(const u8 *png_data, u64 png_data_length) {
    TextureID new_texture_id = Internal::textures.size;
    Internal::textures.append({});
    Ichigo::Internal::gl.glGenTextures(1, &Internal::textures[new_texture_id].id);

    u8 *image_data = stbi_load_from_memory(png_data, png_data_length, (i32 *) &Internal::textures[new_texture_id].width, (i32 *) &Internal::textures[new_texture_id].height, (i32 *) &Internal::textures[new_texture_id].channel_count, 4);
    assert(image_data);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, Internal::textures[new_texture_id].id);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    Ichigo::Internal::gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Internal::textures[new_texture_id].width, Internal::textures[new_texture_id].height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    return new_texture_id;
}

// Decode an MP3. Save the sample data to the permanent storage arena.
// TODO: Allow custom allocators/custom arena output?
Ichigo::AudioID Ichigo::load_audio(const u8 *mp3_data, usize mp3_data_size) {
    AudioID new_audio_id = Internal::audio_assets.size;
    Audio audio;

    drmp3_init_memory(&mp3, mp3_data, mp3_data_size, nullptr);
    u64 frame_count     = drmp3_get_pcm_frame_count(&mp3);
    audio.size_in_bytes = frame_count * mp3.channels * sizeof(i16);
    audio.frames        = PUSH_ARRAY(Ichigo::game_state.permanent_storage_arena, Ichigo::AudioFrame2ChI16LE, frame_count * mp3.channels);
    u64 frames_read     = drmp3_read_pcm_frames_s16(&mp3, frame_count, (i16 *) audio.frames);
    ICHIGO_INFO("Frames read from mp3: %llu frames requested from mp3: %llu", frames_read, frame_count);

    Internal::audio_assets.append(audio);
    return new_audio_id;
}
