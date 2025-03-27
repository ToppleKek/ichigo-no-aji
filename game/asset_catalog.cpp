#include "asset_catalog.hpp"

EMBED("assets/coin.png",                   coin_spritesheet_png)
EMBED("assets/shopkeep.png",               shopkeep_png)
EMBED("assets/select_arrow.png",           select_arrow_png)
EMBED("assets/overworld_tiles_sheet.png",  tileset_png)
EMBED("assets/enemy.png",                  gert_png)
EMBED("assets/music/song.mp3",             test_song_mp3)
EMBED("assets/music/coin.mp3",             coin_sound_mp3)
EMBED("assets/irisu.png",                  irisu_spritesheet_png)
EMBED("assets/music/boo_womp441.mp3",      gert_death_mp3)
EMBED("assets/music/jump.mp3",             jump_sound_mp3)
EMBED("assets/spell.png",                  spell_texture_png)
EMBED("assets/sparks.png",                 sparks_png)
EMBED("assets/key.png",                    key_png)
EMBED("assets/lock.png",                   lock_png)
EMBED("assets/entrance.png",               entrance_png)
EMBED("assets/moving-platform.png",        platform_spritesheet_png)
EMBED("assets/rabbit.png",                 rabbit_texture_png)
EMBED("assets/starbg.png",                 star_bg_png)
EMBED("assets/heart.png",                  heart_png)
EMBED("assets/hp-up.png",                  hp_up_texture_png)
EMBED("assets/attack-speed-up.png",        attack_speed_up_png)
EMBED("assets/fire-spell-collectable.png", fire_spell_collectable_png)
EMBED("assets/fire-spell.png",             fire_spell_png)
EMBED("assets/ice-block.png",              ice_block_png)
EMBED("assets/save-statue.png",            save_statue_png)
EMBED("assets/cavebg.png",                 cave_bg_png)
EMBED("assets/stalactites_bg.png",         stalactites_bg_png)
EMBED("assets/music/steam.mp3",            steam_mp3)
EMBED("assets/music/heart.mp3",            heart_mp3)
EMBED("assets/music/as_up_collect.mp3",    as_up_collect_mp3)
EMBED("assets/music/text_scroll.mp3",      text_scroll_mp3)
EMBED("assets/music/menu_accept.mp3",      menu_accept_mp3)
EMBED("assets/health-bar-ui.png",          health_bar_ui_png)

Ichigo::TextureID Assets::coin_texture_id                   = 0;
Ichigo::TextureID Assets::shopkeep_texture_id               = 0;
Ichigo::TextureID Assets::menu_select_arrow_texture_id      = 0;
Ichigo::TextureID Assets::tileset_texture_id                = 0;
Ichigo::TextureID Assets::gert_texture_id                   = 0;
Ichigo::TextureID Assets::irisu_texture_id                  = 0;
Ichigo::TextureID Assets::spell_texture_id                  = 0;
Ichigo::TextureID Assets::sparks_texture_id                 = 0;
Ichigo::TextureID Assets::key_texture_id                    = 0;
Ichigo::TextureID Assets::lock_texture_id                   = 0;
Ichigo::TextureID Assets::entrance_texture_id               = 0;
Ichigo::TextureID Assets::platform_texture_id               = 0;
Ichigo::TextureID Assets::rabbit_texture_id                 = 0;
Ichigo::TextureID Assets::star_bg_texture_id                = 0;
Ichigo::TextureID Assets::heart_texture_id                  = 0;
Ichigo::TextureID Assets::hp_up_texture_id                  = 0;
Ichigo::TextureID Assets::attack_speed_up_texture_id        = 0;
Ichigo::TextureID Assets::fire_spell_collectable_texture_id = 0;
Ichigo::TextureID Assets::fire_spell_texture_id             = 0;
Ichigo::TextureID Assets::ice_block_texture_id              = 0;
Ichigo::TextureID Assets::save_statue_texture_id            = 0;
Ichigo::TextureID Assets::cave_bg_texture_id                = 0;
Ichigo::TextureID Assets::stalactites_bg_texture_id         = 0;
Ichigo::TextureID Assets::health_bar_ui_texture_id          = 0;
Ichigo::AudioID   Assets::test_song_audio_id                = 0;
Ichigo::AudioID   Assets::coin_collect_audio_id             = 0;
Ichigo::AudioID   Assets::gert_death_audio_id               = 0;
Ichigo::AudioID   Assets::jump_audio_id                     = 0;
Ichigo::AudioID   Assets::steam_audio_id                    = 0;
Ichigo::AudioID   Assets::heart_collect_audio_id            = 0;
Ichigo::AudioID   Assets::as_up_collect_audio_id            = 0;
Ichigo::AudioID   Assets::text_scroll_audio_id              = 0;
Ichigo::AudioID   Assets::menu_accept_audio_id              = 0;

#define LOAD_TEXTURE(ID_NAME, FILE_NAME) Assets::ID_NAME = Ichigo::load_texture(FILE_NAME, FILE_NAME##_len)
#define LOAD_AUDIO(ID_NAME, FILE_NAME)   Assets::ID_NAME = Ichigo::load_audio(FILE_NAME, FILE_NAME##_len)

void Assets::load_assets() {
    LOAD_TEXTURE(coin_texture_id,                   coin_spritesheet_png);
    LOAD_TEXTURE(shopkeep_texture_id,               shopkeep_png);
    LOAD_TEXTURE(menu_select_arrow_texture_id,      select_arrow_png);
    LOAD_TEXTURE(tileset_texture_id,                tileset_png);
    LOAD_TEXTURE(gert_texture_id,                   gert_png);
    LOAD_TEXTURE(irisu_texture_id,                  irisu_spritesheet_png);
    LOAD_TEXTURE(spell_texture_id,                  spell_texture_png);
    LOAD_TEXTURE(sparks_texture_id,                 sparks_png);
    LOAD_TEXTURE(key_texture_id,                    key_png);
    LOAD_TEXTURE(lock_texture_id,                   lock_png);
    LOAD_TEXTURE(entrance_texture_id,               entrance_png);
    LOAD_TEXTURE(platform_texture_id,               platform_spritesheet_png);
    LOAD_TEXTURE(rabbit_texture_id,                 rabbit_texture_png);
    LOAD_TEXTURE(star_bg_texture_id,                star_bg_png);
    LOAD_TEXTURE(heart_texture_id,                  heart_png);
    LOAD_TEXTURE(hp_up_texture_id,                  hp_up_texture_png);
    LOAD_TEXTURE(attack_speed_up_texture_id,        attack_speed_up_png);
    LOAD_TEXTURE(fire_spell_collectable_texture_id, fire_spell_collectable_png);
    LOAD_TEXTURE(fire_spell_texture_id,             fire_spell_png);
    LOAD_TEXTURE(ice_block_texture_id,              ice_block_png);
    LOAD_TEXTURE(save_statue_texture_id,            save_statue_png);
    LOAD_TEXTURE(cave_bg_texture_id,                cave_bg_png);
    LOAD_TEXTURE(stalactites_bg_texture_id,         stalactites_bg_png);
    LOAD_TEXTURE(health_bar_ui_texture_id,          health_bar_ui_png);
    LOAD_AUDIO(test_song_audio_id,                  test_song_mp3);
    LOAD_AUDIO(coin_collect_audio_id,               coin_sound_mp3);
    LOAD_AUDIO(gert_death_audio_id,                 gert_death_mp3);
    LOAD_AUDIO(jump_audio_id,                       jump_sound_mp3);
    LOAD_AUDIO(steam_audio_id,                      steam_mp3);
    LOAD_AUDIO(heart_collect_audio_id,              heart_mp3);
    LOAD_AUDIO(as_up_collect_audio_id,              as_up_collect_mp3);
    LOAD_AUDIO(text_scroll_audio_id,                text_scroll_mp3);
    LOAD_AUDIO(menu_accept_audio_id,                menu_accept_mp3);
}
