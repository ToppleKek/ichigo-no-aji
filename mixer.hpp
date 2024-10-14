#pragma once
#include "asset.hpp"

namespace Ichigo {
namespace Mixer {
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
    u64 frame_play_cursor;
    bool is_playing; // TODO: Stupid?
};
extern Util::IchigoVector<PlayingAudio> playing_audio;
// END DEBUG (we just wanted to see this in the debug window)

PlayingAudioID play_audio(AudioID audio_id);
void cancel_audio(PlayingAudioID id);
PlayingAudio *get_playing_audio(PlayingAudioID id);
void mix_into_buffer(AudioFrame2ChI16LE *sound_buffer, usize buffer_size, usize write_cursor_position_delta);
}
}