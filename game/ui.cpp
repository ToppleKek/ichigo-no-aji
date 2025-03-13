#include "ui.hpp"
#include "ichiaji.hpp"
#include "asset_catalog.hpp"
#include "strings.hpp"

enum MenuType {
    MT_NOTHING,
    MT_SHOP,
    MT_DIALOGUE,
};

struct ShopItem {
    const char *name;
    u32 cost;
    u64 flag;
};

static const ShopItem SHOP_ITEMS[] = {
    {"HP UP",           100, Ichiaji::INV_SHOP_HEALTH_UPGRADE},
    {"Attack Speed UP", 200, Ichiaji::INV_SHOP_ATTACK_SPEED_UPGRADE},
    {"Attack Power UP", 100, Ichiaji::INV_SHOP_ATTACK_POWER_UPGRADE},
};

static MenuType        currently_open_menu           = MT_NOTHING;
static const StringID *current_dialogue_strings      = nullptr;
static isize           current_dialogue_string_count = 0;

void Ui::render_and_update_current_menu() {
    switch (currently_open_menu) {
        case MT_NOTHING: {
            ICHIGO_ERROR("render_current_menu() called with no menu open!");
            Ichiaji::program_state = Ichiaji::PS_GAME;
        } break;

        case MT_SHOP: {
            static Rect<f32> shop_background_rect = {
                {1.0f, 1.0f},
                Ichigo::Camera::screen_tile_dimensions.x - 2.0f,
                Ichigo::Camera::screen_tile_dimensions.y - 2.0f,
            };

            static Ichigo::TextStyle shop_text_style = {
                Ichigo::TextAlignment::LEFT,
                2.2f,
                {1.0f, 1.0f, 1.0f, 1.0f},
                0.0f
            };

            static Ichigo::TextStyle shop_item_text_style = {
                Ichigo::TextAlignment::LEFT,
                1.0f,
                {1.0f, 1.0f, 1.0f, 1.0f},
                0.0f
            };

            Ichigo::render_rect_deferred(Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX, shop_background_rect, {0.0f, 0.0f, 0.0f, 0.95f});

            Ichigo::DrawCommand shop_text_cmd = {
                .type              = Ichigo::DrawCommandType::TEXT,
                .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
                .transform         = m4identity_f32,
                .string            = TL_STR(SHOP_HEADER),
                .string_length     = std::strlen(TL_STR(SHOP_HEADER)),
                .string_pos        = {1.3f, 1.8f},
                .text_style        = shop_text_style
            };

            static u64 current_shopkeep_text_key = SHOPKEEP_WELCOME;

            Ichigo::DrawCommand shopkeep_text_cmd = {
                .type              = Ichigo::DrawCommandType::TEXT,
                .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
                .transform         = m4identity_f32,
                .string            = TL_STR(current_shopkeep_text_key),
                .string_length     = std::strlen(TL_STR(current_shopkeep_text_key)),
                .string_pos        = {1.3f, shop_background_rect.h + 0.6f},
                .text_style        = shop_text_style
            };

            static Ichigo::Sprite coin_sprite = {
                .pos_offset = {0.0f, 0.0f},
                .width      = pixels_to_metres(32),
                .height     = pixels_to_metres(32),
                .sheet      = {
                    .cell_width  = 32,
                    .cell_height = 32,
                    .texture     = Assets::coin_texture_id,
                },
                .animation  = {
                    .tag                 = 0,
                    .cell_of_first_frame = 0,
                    .cell_of_last_frame  = 7,
                    .cell_of_loop_start  = 0,
                    .cell_of_loop_end    = 7,
                    .seconds_per_frame   = 0.2f
                },
                .current_animation_frame      = 0,
                .elapsed_animation_frame_time = 0.0f,
            };

            Ichigo::DrawCommand currency_sprite_cmd = {
                .type                   = Ichigo::DrawCommandType::SPRITE,
                .coordinate_system      = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
                .transform              = m4identity_f32,
                .sprite                 = &coin_sprite,
                .sprite_pos             = {shop_background_rect.w - 3.0f, 1.2f},
                .sprite_flip_h          = false,
                .actually_render_sprite = true
            };

            Bana::String currency = make_string(64, Ichigo::Internal::temp_allocator);
            Bana::string_format(currency, "%u", Ichiaji::current_save_data.player_data.money);

            Ichigo::DrawCommand currency_text_cmd = {
                .type              = Ichigo::DrawCommandType::TEXT,
                .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
                .transform         = m4identity_f32,
                .string            = currency.data,
                .string_length     = currency.length,
                .string_pos        = {shop_background_rect.w - 1.5f, 2.0f},
                .text_style        = shop_text_style
            };

            static Ichigo::Texture &shopkeep_texture     = Ichigo::Internal::textures[Assets::shopkeep_texture_id];
            static Ichigo::Texture &select_arrow_texture = Ichigo::Internal::textures[Assets::menu_select_arrow_texture_id];
            Ichigo::DrawCommand shopkeep_cmd = {
                .type              = Ichigo::DrawCommandType::TEXTURED_RECT,
                .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
                .transform         = m4identity_f32,
                .texture_rect      = {{shop_background_rect.w / 1.3f, 2.0f}, pixels_to_metres(shopkeep_texture.width), pixels_to_metres(shopkeep_texture.height)},
                .texture_id        = Assets::shopkeep_texture_id,
                .texture_tint      = COLOUR_WHITE
            };

            static i32 selected_item = 0;
            for (i32 i = 0; i < ARRAY_LEN(SHOP_ITEMS); ++i) {
                Bana::String str = make_string(64, Ichigo::Internal::temp_allocator);
                if (FLAG_IS_SET(Ichiaji::current_save_data.player_data.inventory_flags, SHOP_ITEMS[i].flag)) {
                    Bana::string_concat(str, TL_STR(SHOP_SOLD_OUT));
                } else {
                    // TODO: Translate item names too.
                    Bana::string_format(str, "%s - %u", SHOP_ITEMS[i].name, SHOP_ITEMS[i].cost);
                }

                if (i == selected_item) {
                    f64 t = std::sin(Ichigo::Internal::platform_get_current_time() * 2);
                    t *= t;
                    t = 0.5f + 0.5f * t;

                    Ichigo::DrawCommand select_arrow_cmd = {
                        .type              = Ichigo::DrawCommandType::TEXTURED_RECT,
                        .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
                        .transform         = m4identity_f32,
                        .texture_rect      = {{ichigo_lerp(1.15f, t, 1.3f), 3.0f + i - pixels_to_metres(select_arrow_texture.height)}, pixels_to_metres(select_arrow_texture.width), pixels_to_metres(select_arrow_texture.height)},
                        .texture_id        = Assets::menu_select_arrow_texture_id,
                        .texture_tint      = COLOUR_WHITE
                    };


                    Ichigo::push_draw_command(select_arrow_cmd);
                }

                Ichigo::DrawCommand text_cmd = {
                    .type              = Ichigo::DrawCommandType::TEXT,
                    .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
                    .transform         = m4identity_f32,
                    .string            = str.data,
                    .string_length     = str.length,
                    .string_pos        = {2.0f, 3.0f + i},
                    .text_style        = shop_item_text_style
                };

                Ichigo::push_draw_command(text_cmd);
            }

            Ichigo::push_draw_command(shop_text_cmd);
            Ichigo::push_draw_command(shopkeep_text_cmd);
            Ichigo::push_draw_command(currency_sprite_cmd);
            Ichigo::push_draw_command(currency_text_cmd);
            Ichigo::push_draw_command(shopkeep_cmd);

            if (Ichigo::Internal::keyboard_state[Ichigo::IK_ESCAPE].down_this_frame || Ichigo::Internal::gamepad.start.down_this_frame) {
                currently_open_menu       = MT_NOTHING;
                selected_item             = 0;
                Ichiaji::program_state    = Ichiaji::PS_GAME;
                current_shopkeep_text_key = SHOPKEEP_WELCOME;
            }

            if (Ichigo::Internal::keyboard_state[Ichigo::IK_DOWN].down_this_frame || Ichigo::Internal::gamepad.down.down_this_frame) {
                selected_item = clamp(selected_item + 1, 0, (i32) ARRAY_LEN(SHOP_ITEMS) - 1);
            }

            if (Ichigo::Internal::keyboard_state[Ichigo::IK_UP].down_this_frame || Ichigo::Internal::gamepad.up.down_this_frame) {
                selected_item = clamp(selected_item - 1, 0, (i32) ARRAY_LEN(SHOP_ITEMS) - 1);
            }

            if (Ichigo::Internal::keyboard_state[Ichigo::IK_ENTER].down_this_frame || Ichigo::Internal::gamepad.a.down_this_frame) {
                if (FLAG_IS_SET(Ichiaji::current_save_data.player_data.inventory_flags, SHOP_ITEMS[selected_item].flag)) {
                    // TODO @asset: Play a sound effect here.
                    current_shopkeep_text_key = SHOPKEEP_SOLD_OUT;
                } else if (SHOP_ITEMS[selected_item].cost <= Ichiaji::current_save_data.player_data.money) {
                    // TODO @asset: Play a sound effect here.
                    current_shopkeep_text_key = SHOPKEEP_THANK_YOU;
                    Ichiaji::current_save_data.player_data.money -= SHOP_ITEMS[selected_item].cost;
                    SET_FLAG(Ichiaji::current_save_data.player_data.inventory_flags, SHOP_ITEMS[selected_item].flag);
                } else {
                    // TODO @asset: Play a sound effect here.
                    current_shopkeep_text_key = SHOPKEEP_YOU_ARE_BROKE;
                }
            }
        } break;

        case MT_DIALOGUE: {
#define CHARACTER_COOLDOWN 0.03f

            static isize current_string_index    = 0;
            static isize current_string_length = 0;
            static f32   character_cooldown_t    = 0.0f;

            static Ichigo::TextStyle text_style  = {
                Ichigo::TextAlignment::LEFT,
                0.8f,
                {1.0f, 1.0f, 1.0f, 1.0f},
                100.0f
            };

            if (current_string_index >= current_dialogue_string_count || current_dialogue_strings == nullptr || current_dialogue_string_count == 0) {
                current_string_index          = 0;
                current_string_length         = 0;
                character_cooldown_t          = 0.0f;
                current_dialogue_strings      = nullptr;
                current_dialogue_string_count = 0;
                Ichiaji::program_state        = Ichiaji::PS_GAME;
                currently_open_menu           = MT_NOTHING;
            } else {
                const char *string  = TL_STR(current_dialogue_strings[current_string_index]);
                isize string_length = std::strlen(string);

                static Rect<f32> text_background_rect = {
                    {1.0f, 1.0f},
                    Ichigo::Camera::screen_tile_dimensions.x - 2.0f,
                    3.0f,
                };

                Ichigo::render_rect_deferred(Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX, text_background_rect, {0.0f, 0.0f, 0.0f, 1.0f});

                character_cooldown_t += Ichigo::Internal::dt;

                if (character_cooldown_t >= CHARACTER_COOLDOWN) {
                    // TODO @asset: Play some noise when advancing the string cursor.
                    u32 advance_by = (u32) (character_cooldown_t / CHARACTER_COOLDOWN);
                    current_string_length = clamp(current_string_length + advance_by, (isize) 0, (isize) string_length);
                    character_cooldown_t    = 0.0f;
                }

                Ichigo::DrawCommand text_cmd = {
                    .type              = Ichigo::DrawCommandType::TEXT,
                    .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
                    .transform         = m4identity_f32,
                    .string            = string,
                    .string_length     = (usize) current_string_length,
                    .string_pos        = {text_background_rect.pos.x + 0.2f, text_background_rect.pos.y + 0.5f},
                    .text_style        = text_style
                };

                Ichigo::push_draw_command(text_cmd);

                if (Ichigo::Internal::keyboard_state[Ichigo::IK_ENTER].down_this_frame || Ichigo::Internal::gamepad.a.down_this_frame) {
                    // TODO @asset: Play a sound when advancing the text box.
                    if (current_string_length >= string_length) {
                        current_string_length = 0;
                        character_cooldown_t  = 0.0f;
                        ++current_string_index;
                    } else {
                        current_string_length = string_length;
                    }
                }
            }
        } break;
    }
}

void Ui::open_shop_ui() {
    Ichiaji::program_state = Ichiaji::PS_UI_MENU;
    currently_open_menu    = MT_SHOP;
}

void Ui::open_dialogue_ui(const StringID *strings, isize string_count) {
    Ichiaji::program_state        = Ichiaji::PS_UI_MENU;
    currently_open_menu           = MT_DIALOGUE;
    current_dialogue_strings      = strings;
    current_dialogue_string_count = string_count;
}
