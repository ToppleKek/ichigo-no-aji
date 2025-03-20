#include "asset_catalog.hpp"

EMBED("assets/coin.png", coin_spritesheet_png)
EMBED("assets/shopkeep.png", shopkeep_png)
EMBED("assets/select_arrow.png", select_arrow_png)
EMBED("assets/overworld_tiles.png", tileset_png)
EMBED("assets/enemy.png", gert_png)
EMBED("assets/music/song.mp3", test_song_mp3)
EMBED("assets/music/coin.mp3", coin_sound_mp3)
EMBED("assets/irisu.png", irisu_spritesheet_png)
EMBED("assets/music/boo_womp441.mp3", gert_death_mp3)
EMBED("assets/music/jump.mp3", jump_sound_mp3)
EMBED("assets/spell.png", spell_texture_png)
EMBED("assets/sparks.png", sparks_png)
EMBED("assets/key.png", key_png)
EMBED("assets/lock.png", lock_png)
EMBED("assets/entrance.png", entrance_png)
EMBED("assets/moving-platform.png", platform_spritesheet_png)
EMBED("assets/rabbit.png", rabbit_texture_png)

Ichigo::TextureID Assets::coin_texture_id = 0;
Ichigo::TextureID Assets::shopkeep_texture_id = 0;
Ichigo::TextureID Assets::menu_select_arrow_texture_id = 0;
Ichigo::TextureID Assets::tileset_texture_id = 0;
Ichigo::TextureID Assets::gert_texture_id = 0;
Ichigo::TextureID Assets::irisu_texture_id = 0;
Ichigo::TextureID Assets::spell_texture_id = 0;
Ichigo::TextureID Assets::sparks_texture_id = 0;
Ichigo::TextureID Assets::key_texture_id = 0;
Ichigo::TextureID Assets::lock_texture_id = 0;
Ichigo::TextureID Assets::entrance_texture_id = 0;
Ichigo::TextureID Assets::platform_texture_id = 0;
Ichigo::TextureID Assets::rabbit_texture_id = 0;
Ichigo::AudioID   Assets::test_song_audio_id = 0;
Ichigo::AudioID   Assets::coin_collect_audio_id = 0;
Ichigo::AudioID   Assets::gert_death_audio_id = 0;
Ichigo::AudioID   Assets::jump_audio_id = 0;

#define LOAD_TEXTURE(ID_NAME, FILE_NAME) Assets::ID_NAME = Ichigo::load_texture(FILE_NAME, FILE_NAME##_len)
#define LOAD_AUDIO(ID_NAME, FILE_NAME)   Assets::ID_NAME = Ichigo::load_audio(FILE_NAME, FILE_NAME##_len)

void Assets::load_assets() {
    LOAD_TEXTURE(coin_texture_id, coin_spritesheet_png);
    LOAD_TEXTURE(shopkeep_texture_id, shopkeep_png);
    LOAD_TEXTURE(menu_select_arrow_texture_id, select_arrow_png);
    LOAD_TEXTURE(tileset_texture_id, tileset_png);
    LOAD_TEXTURE(gert_texture_id, gert_png);
    LOAD_TEXTURE(irisu_texture_id, irisu_spritesheet_png);
    LOAD_TEXTURE(spell_texture_id, spell_texture_png);
    LOAD_TEXTURE(sparks_texture_id, sparks_png);
    LOAD_TEXTURE(key_texture_id, key_png);
    LOAD_TEXTURE(lock_texture_id, lock_png);
    LOAD_TEXTURE(entrance_texture_id, entrance_png);
    LOAD_TEXTURE(platform_texture_id, platform_spritesheet_png);
    LOAD_TEXTURE(rabbit_texture_id, rabbit_texture_png);
    LOAD_AUDIO(test_song_audio_id,    test_song_mp3);
    LOAD_AUDIO(coin_collect_audio_id, coin_sound_mp3);
    LOAD_AUDIO(gert_death_audio_id, gert_death_mp3);
    LOAD_AUDIO(jump_audio_id, jump_sound_mp3);
}
