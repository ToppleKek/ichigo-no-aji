/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Audio mixer.

    Author:      Braeden Hong
    Last edited: 2025/03/28
*/

#include "mixer.hpp"
#include "math.hpp"

#include <emmintrin.h>
#include <smmintrin.h>

// TODO: @heap
Bana::Array<Ichigo::Mixer::PlayingAudio> Ichigo::Mixer::playing_audio{64};
f32 Ichigo::Mixer::master_volume = 1.0f;
// TODO: Use transient storage?
// static Ichigo::AudioFrame2ChI16LE mix_buffer[MEGABYTES(50)];

Ichigo::Mixer::PlayingAudioID Ichigo::Mixer::play_audio(AudioID audio_id, f32 volume, f32 left_volume, f32 right_volume, f32 loop_start_seconds, f32 loop_end_seconds) {
    PlayingAudio pa = {};
    pa.audio_id          = audio_id;
    pa.volume            = volume;
    pa.left_volume       = left_volume;
    pa.right_volume      = right_volume;
    pa.loop_start_frame  = (u64) (loop_start_seconds * AUDIO_SAMPLE_RATE);
    pa.loop_end_frame    = (u64) (loop_end_seconds   * AUDIO_SAMPLE_RATE);
    pa.frame_play_cursor = 0;

    // Find some slot to add this audio.
    for (u32 i = 0; i < playing_audio.size; ++i) {
        if (playing_audio[i].audio_id == 0) {
            pa.id.generation = playing_audio[i].id.generation + 1;
            pa.id.index      = i;
            std::memcpy(&playing_audio[i], &pa, sizeof(pa));
            return pa.id;
        }
    }

    // No leftovers for us. Make a new slot.
    pa.id.generation = 0;
    pa.id.index      = playing_audio.size;
    playing_audio.append(pa);

    return pa.id;
}

Ichigo::Mixer::PlayingAudioID Ichigo::Mixer::play_audio_oneshot(AudioID audio_id, f32 volume, f32 left_volume, f32 right_volume) {
    return play_audio(audio_id, volume, left_volume, right_volume, 0.0f, 0.0f);
}

Ichigo::Mixer::PlayingAudio *Ichigo::Mixer::get_playing_audio(PlayingAudioID id) {
    if (id.index >= Ichigo::Mixer::playing_audio.size)
        return nullptr;

    PlayingAudio *pa = &Ichigo::Mixer::playing_audio[id.index];

    // TODO: See other TODO after this, but for now we just know that if a PlayingAudio has an audio asset id of 0 then it is invalid.
    //       Should we instead do what everything else does and have id.index == 0 be the "null playing audio" and have that be invalid?
    if (pa->id.generation != id.generation || pa->audio_id == 0)
        return nullptr;

    return &Ichigo::Mixer::playing_audio[id.index];
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
        PlayingAudio &pa = playing_audio[i];
        if (pa.audio_id == 0)
            continue;

        Audio &audio = Internal::audio_assets[pa.audio_id];

        // Only advance the play cursor and all that stuff if the audio has started playing. If we didn't guard this then audio would skip the first "write_cursor_position_delta" bytes of the audio when being played.
        if (pa.is_playing) {
            pa.frame_play_cursor += write_cursor_position_delta / sizeof(AudioFrame2ChI16LE);
            if (pa.loop_end_frame != 0 && pa.frame_play_cursor >= pa.loop_end_frame) {
                pa.frame_play_cursor = pa.loop_start_frame;
            }

            // The audio has finished playing.
            if (pa.frame_play_cursor * sizeof(AudioFrame2ChI16LE) >= audio.size_in_bytes) {
                pa.audio_id = 0;
                continue;
            }
        } else {
            pa.is_playing = true;
        }

        // u64 start = __rdtsc();
        // f32 left_channel_volume  = pa.volume * pa.left_volume  * master_volume;
        // f32 right_channel_volume = pa.volume * pa.right_volume * master_volume;
        // for (u32 audio_frame_index = pa.frame_play_cursor, i = 0; audio_frame_index < audio.size_in_bytes / sizeof(AudioFrame2ChI16LE) && i < buffer_size / sizeof(AudioFrame2ChI16LE); ++audio_frame_index, ++i) {
        //     // TODO: Mix into i32 buffer, and clamp after that
        //     i32 mixed_value_l = sound_buffer[i].l + audio.frames[audio_frame_index].l * left_channel_volume;
        //     i32 mixed_value_r = sound_buffer[i].r + audio.frames[audio_frame_index].r * right_channel_volume;
        //     mixed_value_l = clamp(mixed_value_l, (i32) INT16_MIN, (i32) INT16_MAX);
        //     mixed_value_r = clamp(mixed_value_r, (i32) INT16_MIN, (i32) INT16_MAX);
        //     sound_buffer[i].l = mixed_value_l;
        //     sound_buffer[i].r = mixed_value_r;
        // }
        // ICHIGO_INFO("MIX: cycle count: %llu", __rdtsc() - start);

        // u64 start = __rdtsc();
        f32 left_channel_volume  = pa.volume * pa.left_volume  * master_volume;
        f32 right_channel_volume = pa.volume * pa.right_volume * master_volume;
        // Audio frames are processed two at a time. We load the left and right volumes alternating here because the channels of the two frames will be processed L R L R
        __m128 volume4x  = _mm_setr_ps(left_channel_volume, right_channel_volume, left_channel_volume, right_channel_volume);
        for (u32 audio_frame_index = pa.frame_play_cursor, i = 0; audio_frame_index < audio.size_in_bytes / sizeof(AudioFrame2ChI16LE) && i < buffer_size / sizeof(AudioFrame2ChI16LE); audio_frame_index += 2, i += 2) {
            // Load 128 bits of audio frame data. This loads 4 frames, but we can only process 2 of them.
            __m128i frames32         = _mm_loadu_si128((__m128i *) &audio.frames[audio_frame_index]);
            // pmovsxwd xmm, xmm - Sign extend packed i16 to packed i32
            // cvtdq2ps xmm, xmm - Convert packed i32 to packed f32
            // "frames" is now 2 audio frames, converted to floats.
            __m128 frames            = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(frames32));
            // Load and convert the sound buffer in the same way.
            __m128i sb32             = _mm_loadu_si128((__m128i *) &sound_buffer[i]);
            __m128 sb                = _mm_cvtepi32_ps(_mm_cvtepi16_epi32(sb32));
            // Apply volume adjustments to the current audio frames.
            __m128 processed_frames  = _mm_mul_ps(frames, volume4x);
            // Mix the final value
            __m128 mixed_value       = _mm_add_ps(sb, processed_frames);
            // Store the result back to memory.
            mixed_value              = _mm_cvtps_epi32(mixed_value);
            // NOTE: packssdw xmm, xmm - Signed saturation pack of 16 bit values. This effectively clamps for free.
            mixed_value              = _mm_packs_epi32(mixed_value, mixed_value);
            _mm_storel_epi64((__m128i *) &sound_buffer[i], mixed_value);
        }
        // ICHIGO_INFO("MIX: cycle count: %llu", __rdtsc() - start);
    }
}
