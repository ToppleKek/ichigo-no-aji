/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Audio mixer.

    Author:      Braeden Hong
    Last edited: 2024/11/30
*/

#pragma once
#include "asset.hpp"

namespace Ichigo {
namespace Mixer {

// Same generation/index format as entity IDs.
struct PlayingAudioID {
    u32 generation;
    u32 index;
};

// DEBUG (MOVE BACK INTO MIXER)
// TODO: Panning, fading, pitch, etc.
struct PlayingAudio {
    PlayingAudioID id;
    Ichigo::AudioID audio_id;
    f32 volume;
    f32 left_volume;
    f32 right_volume;
    u64 loop_start_frame;
    u64 loop_end_frame;
    u64 frame_play_cursor;
    bool is_playing; // TODO: Stupid?
};
extern Bana::Array<PlayingAudio> playing_audio;
extern f32 master_volume;
// END DEBUG (we just wanted to see this in the debug window)

// Play audio that loops.
PlayingAudioID play_audio(AudioID audio_id, f32 volume, f32 left_volume, f32 right_volume, f32 loop_start_seconds, f32 loop_end_seconds);

// Play a sound effect that will not loop.
PlayingAudioID play_audio_oneshot(AudioID audio_id, f32 volume, f32 left_volume, f32 right_volume);

// Stop playing a sound.
void cancel_audio(PlayingAudioID id);
PlayingAudio *get_playing_audio(PlayingAudioID id);

// Called to actually perform audio mixing. The engine calls this to fill the platform's sample buffer with new samples to be sent to the sound card.
// The "write_cursor_position_delta" is how far the "write cursor" has actually moved since the last time we called this function.
// The reason we need this is that we usually actually mix way more audio than we have to in order to guard against the audio skipping if our frame
// rate drops for whatever reason. We use the write cursor delta to tell us how many audio samples have already been sent to the sound card
// so that we can advance our "play cursor". This prevents us from mixing audio that was already uploaded.
void mix_into_buffer(AudioFrame2ChI16LE *sound_buffer, usize buffer_size, usize write_cursor_position_delta);
}
}