#include "mixer.hpp"
#include "math.hpp"

Util::IchigoVector<Ichigo::Mixer::PlayingAudio> Ichigo::Mixer::playing_audio{64};
f32 Ichigo::Mixer::master_volume = 1.0f;
// TODO: Use transient storage?
// static Ichigo::AudioFrame2ChI16LE mix_buffer[MEGABYTES(50)];

Ichigo::Mixer::PlayingAudioID Ichigo::Mixer::play_audio(AudioID audio_id, f32 volume, f32 left_volume, f32 right_volume) {
    PlayingAudio pa;
    pa.audio_id           = audio_id;
    pa.volume             = volume;
    pa.left_volume        = left_volume;
    pa.right_volume       = right_volume;
    pa.frame_play_cursor  = 0;

    for (u32 i = 0; i < playing_audio.size; ++i) {
        if (playing_audio.at(i).audio_id == 0) {
            pa.id.generation = playing_audio.at(i).id.generation + 1;
            pa.id.index      = i;
            std::memcpy(&playing_audio.at(i), &pa, sizeof(pa));
            return pa.id;
        }
    }

    pa.id.generation = 0;
    pa.id.index      = playing_audio.size;
    playing_audio.append(pa);

    return pa.id;
}

Ichigo::Mixer::PlayingAudio *Ichigo::Mixer::get_playing_audio(PlayingAudioID id) {
    if (id. index >= Ichigo::Mixer::playing_audio.size)
        return nullptr;

    PlayingAudio *pa = &Ichigo::Mixer::playing_audio.at(id.index);

    // TODO: See other TODO after this, but for now we just know that if a PlayingAudio has an audio asset id of 0 then it is invalid.
    //       Should we instead do what everything else does and have id.index == 0 be the "null playing audio" and have that be invalid?
    if (pa->id.generation != id.generation || pa->audio_id == 0)
        return nullptr;

    return &Ichigo::Mixer::playing_audio.at(id.index);
}

void Ichigo::Mixer::cancel_audio(PlayingAudioID id) {
    PlayingAudio *pa = get_playing_audio(id);
    if (!pa)
        return;

    // NOTE: In the mixer, if the audio of a PlayingAudio is 0, then the slot is open.
    // TODO: Should there be a "null playing audio (id.index = 0)"? There is for entities, audio, textures, etc...
    //       BUT, we have other ways of telling if the playing audio is invalid or not.
    pa->audio_id = 0;
}

void Ichigo::Mixer::mix_into_buffer(AudioFrame2ChI16LE *sound_buffer, usize buffer_size, usize write_cursor_position_delta) {
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
            i32 mixed_value_l = sound_buffer[i].l + audio.frames[audio_frame_index].l * pa.volume * pa.left_volume  * master_volume;
            i32 mixed_value_r = sound_buffer[i].r + audio.frames[audio_frame_index].r * pa.volume * pa.right_volume * master_volume;
            mixed_value_l = clamp(mixed_value_l, (i32) INT16_MIN, (i32) INT16_MAX);
            mixed_value_r = clamp(mixed_value_r, (i32) INT16_MIN, (i32) INT16_MAX);
            sound_buffer[i].l = mixed_value_l;
            sound_buffer[i].r = mixed_value_r;
        }
    }
}

