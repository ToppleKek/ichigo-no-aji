#include "asset.hpp"
#include "ichigo.hpp"

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static drmp3 mp3;

Util::IchigoVector<Ichigo::Texture> Ichigo::Internal::textures{64};
Util::IchigoVector<Ichigo::Audio> Ichigo::Internal::audio_assets{64};

Ichigo::TextureID Ichigo::load_texture(const u8 *png_data, u64 png_data_length) {
    TextureID new_texture_id = Internal::textures.size;
    Internal::textures.append({});
    Ichigo::Internal::gl.glGenTextures(1, &Internal::textures.at(new_texture_id).id);

    u8 *image_data = stbi_load_from_memory(png_data, png_data_length, (i32 *) &Internal::textures.at(new_texture_id).width, (i32 *) &Internal::textures.at(new_texture_id).height, (i32 *) &Internal::textures.at(new_texture_id).channel_count, 4);
    assert(image_data);
    Ichigo::Internal::gl.glBindTexture(GL_TEXTURE_2D, Internal::textures.at(new_texture_id).id);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    Ichigo::Internal::gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    Ichigo::Internal::gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Internal::textures.at(new_texture_id).width, Internal::textures.at(new_texture_id).height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    Ichigo::Internal::gl.glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(image_data);

    return new_texture_id;
}

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
