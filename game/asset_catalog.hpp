#pragma once

#include "../ichigo.hpp"

namespace Assets {
extern Ichigo::TextureID coin_texture_id;
extern Ichigo::TextureID shopkeep_texture_id;
extern Ichigo::TextureID menu_select_arrow_texture_id;
extern Ichigo::TextureID tileset_texture_id;
extern Ichigo::TextureID gert_texture_id;
extern Ichigo::TextureID irisu_texture_id;
extern Ichigo::TextureID spell_texture_id;
extern Ichigo::TextureID sparks_texture_id;
extern Ichigo::TextureID key_texture_id;
extern Ichigo::TextureID lock_texture_id;
extern Ichigo::TextureID entrance_texture_id;
extern Ichigo::TextureID platform_texture_id;
extern Ichigo::TextureID rabbit_texture_id;
extern Ichigo::TextureID star_bg_texture_id;
extern Ichigo::AudioID   test_song_audio_id;
extern Ichigo::AudioID   coin_collect_audio_id;
extern Ichigo::AudioID   gert_death_audio_id;
extern Ichigo::AudioID   jump_audio_id;

void load_assets();
}
