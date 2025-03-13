#include "asset_catalog.hpp"

EMBED("assets/coin.png", coin_spritesheet_png)
EMBED("assets/shopkeep.png", shopkeep_png)
EMBED("assets/select_arrow.png", select_arrow_png)

Ichigo::TextureID Assets::coin_texture_id = 0;
Ichigo::TextureID Assets::shopkeep_texture_id = 0;
Ichigo::TextureID Assets::menu_select_arrow_texture_id = 0;

void Assets::load_assets() {
    Assets::coin_texture_id              = Ichigo::load_texture(coin_spritesheet_png, coin_spritesheet_png_len);
    Assets::shopkeep_texture_id          = Ichigo::load_texture(shopkeep_png, shopkeep_png_len);
    Assets::menu_select_arrow_texture_id = Ichigo::load_texture(select_arrow_png, select_arrow_png_len);
}
