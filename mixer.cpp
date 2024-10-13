#include "mixer.hpp"
#include "math.hpp"

Util::IchigoVector<Ichigo::Mixer::PlayingAudio> Ichigo::Mixer::playing_audio{64};
// TODO: Use transient storage?
// static Ichigo::AudioFrame2ChI16LE mix_buffer[MEGABYTES(50)];

void Ichigo::Mixer::play_audio(AudioID audio_id) {
    PlayingAudio pa;
    pa.audio_id          = audio_id;
    pa.volume            = 1.0f; // TODO: Custom volume
    pa.frame_play_cursor = 0;

    for (u32 i = 0; i < playing_audio.size; ++i) {
        if (playing_audio.at(i).audio_id == 0) {
            std::memcpy(&playing_audio.at(i), &pa, sizeof(pa));
            return;
        }
    }

    playing_audio.append(pa);
}

void Ichigo::Mixer::mix_into_buffer(AudioFrame2ChI16LE *sound_buffer, usize buffer_size, usize write_cursor_position_delta) {
    ICHIGO_INFO("write_cursor_position_delta=%llu ?? %d", write_cursor_position_delta, write_cursor_position_delta % sizeof(AudioFrame2ChI16LE));
    for (u32 i = 0; i < playing_audio.size; ++i) {
        PlayingAudio &pa = playing_audio.at(i);
        if (pa.audio_id == 0)
            continue;

        Audio audio = Internal::audio_assets.at(pa.audio_id);

        if (pa.is_playing) {
            pa.frame_play_cursor += write_cursor_position_delta / sizeof(AudioFrame2ChI16LE);
            if (pa.frame_play_cursor * sizeof(AudioFrame2ChI16LE) >= audio.size_in_bytes) {
                pa.audio_id = 0;
                continue;
            }
        } else {
            pa.is_playing = true;
        }

        for (u32 audio_frame_index = pa.frame_play_cursor, i = 0; audio_frame_index < audio.size_in_bytes / sizeof(AudioFrame2ChI16LE) && i < buffer_size / sizeof(AudioFrame2ChI16LE); ++audio_frame_index, ++i) {
            // TODO: Mix into i32 buffer, and clamp after that
            i32 mixed_value_l = sound_buffer[i].l + audio.frames[audio_frame_index].l;
            i32 mixed_value_r = sound_buffer[i].r + audio.frames[audio_frame_index].r;
            mixed_value_l = clamp(mixed_value_l, (i32) INT16_MIN, (i32) INT16_MAX);
            mixed_value_r = clamp(mixed_value_r, (i32) INT16_MIN, (i32) INT16_MAX);
            sound_buffer[i].l = mixed_value_l;
            sound_buffer[i].r = mixed_value_r;
        }
    }
}

