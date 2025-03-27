#include "ui.hpp"
#include "ichiaji.hpp"
#include "asset_catalog.hpp"
#include "strings.hpp"

enum MenuType {
    MT_NOTHING,
    MT_SHOP,
    MT_DIALOGUE,
    MT_GAME_OVER,
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
                .sprite_tint            = COLOUR_WHITE,
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
            for (i32 i = 0; i < (i32) ARRAY_LEN(SHOP_ITEMS); ++i) {
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
                    Ichiaji::recalculate_player_bonuses();
                } else {
                    // TODO @asset: Play a sound effect here.
                    current_shopkeep_text_key = SHOPKEEP_YOU_ARE_BROKE;
                }
            }
        } break;

        case MT_DIALOGUE: {
#define CHARACTER_COOLDOWN 0.03f

            static isize current_string_index  = 0;
            static isize current_string_length = 0;
            static f32   character_cooldown_t  = 0.0f;

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
                    u32 advance_by        = (u32) (character_cooldown_t / CHARACTER_COOLDOWN);
                    current_string_length = clamp(current_string_length + advance_by, (isize) 0, (isize) string_length);
                    character_cooldown_t  = 0.0f;

                    if (current_string_length != string_length && current_string_length % 2 == 0) {
                        Ichigo::Mixer::play_audio_oneshot(Assets::text_scroll_audio_id, 0.75f, 1.0f, 1.0f);
                    }
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
                    Ichigo::Mixer::play_audio_oneshot(Assets::menu_accept_audio_id, 1.0f, 1.0f, 1.0f);

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

        case MT_GAME_OVER: {
            static constexpr u32 MENU_ITEM_COUNT = 2;
            static u32 selected_menu_item = 0;

            static Ichigo::TextStyle text_style = {
                .alignment    = Ichigo::TextAlignment::CENTER,
                .scale        = 1.2f,
                .colour       = {0.0f, 0.0f, 0.0f, 1.0f},
                .line_spacing = 100.0f
            };

            static Ichigo::TextStyle menu_item_style = {
                .alignment    = Ichigo::TextAlignment::CENTER,
                .scale        = 1.0f,
                .colour       = {0.0f, 0.0f, 0.0f, 1.0f},
                .line_spacing = 100.0f
            };

            Ichigo::DrawCommand text_cmd = {
                .type              = Ichigo::DrawCommandType::TEXT,
                .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
                .transform         = m4identity_f32,
                .string            = TL_STR(GAME_OVER_TEXT),
                .string_length     = std::strlen(TL_STR(GAME_OVER_TEXT)),
                .string_pos        = {SCREEN_ASPECT_FIX_WIDTH / 2.0f, 2.0f},
                .text_style        = text_style
            };

            static Ichigo::DrawCommand background_cmd = {
                .type              = Ichigo::DrawCommandType::SOLID_COLOUR_RECT,
                .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
                .transform         = m4identity_f32,
                .rect              = {{0.0f, 0.0f}, SCREEN_ASPECT_FIX_WIDTH, SCREEN_ASPECT_FIX_HEIGHT},
                .colour            = COLOUR_WHITE
            };

            Ichigo::push_draw_command(background_cmd);
            Ichigo::push_draw_command(text_cmd);

            bool menu_down_button_down_this_frame   = Ichigo::Internal::keyboard_state[Ichigo::IK_DOWN].down_this_frame || Ichigo::Internal::gamepad.down.down_this_frame;
            bool menu_up_button_down_this_frame     = Ichigo::Internal::keyboard_state[Ichigo::IK_UP].down_this_frame || Ichigo::Internal::gamepad.up.down_this_frame;
            bool menu_select_button_down_this_frame = Ichigo::Internal::keyboard_state[Ichigo::IK_ENTER].down_this_frame || Ichigo::Internal::gamepad.a.down_this_frame || Ichigo::Internal::gamepad.start.down_this_frame;

            if (menu_down_button_down_this_frame) {
                selected_menu_item = (selected_menu_item + 1) % MENU_ITEM_COUNT;
            } else if (menu_up_button_down_this_frame) {
                selected_menu_item = DEC_POSITIVE_OR(selected_menu_item, MENU_ITEM_COUNT - 1);
            }

            f64 t = std::sin(Ichigo::Internal::platform_get_current_time() * 2.0);
            t *= t;
            t = 0.5 + 0.5 * t;
            Vec4<f32> pulse_colour = {0.6f, 0.2f, ichigo_lerp(0.4, t, 0.9), 1.0f};

            Ichigo::DrawCommand menu_draw_cmd = {
                .type              = Ichigo::TEXT,
                .coordinate_system = Ichigo::CoordinateSystem::SCREEN_ASPECT_FIX,
                .transform         = m4identity_f32,
                .string            = TL_STR(GAME_OVER_LOAD_GAME),
                .string_length     = std::strlen(TL_STR(GAME_OVER_LOAD_GAME)),
                .string_pos        = {SCREEN_ASPECT_FIX_WIDTH / 2.0f, SCREEN_ASPECT_FIX_HEIGHT - 3.0f},
                .text_style        = menu_item_style
            };

            if (selected_menu_item == 0) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = {0.0f, 0.0f, 0.0f, 1.0f};


            Ichigo::push_draw_command(menu_draw_cmd);

            if (selected_menu_item == 1) menu_draw_cmd.text_style.colour = pulse_colour;
            else                         menu_draw_cmd.text_style.colour = {0.0f, 0.0f, 0.0f, 1.0f};

            menu_draw_cmd.string        = TL_STR(EXIT);
            menu_draw_cmd.string_length = std::strlen(TL_STR(EXIT)),
            menu_draw_cmd.string_pos.y  = SCREEN_ASPECT_FIX_HEIGHT - 2.0f;

            Ichigo::push_draw_command(menu_draw_cmd);

            if (menu_select_button_down_this_frame) {
                if (selected_menu_item == 0) {
                    selected_menu_item     = 0;
                    currently_open_menu    = MT_NOTHING;
                    Ichiaji::program_state = Ichiaji::PS_GAME;
                    Ichiaji::restart_game_from_save_on_disk();
                    Ichiaji::current_save_data.player_data.health = PLAYER_STARTING_HEALTH + Ichiaji::player_bonuses.max_health;
                } else if (selected_menu_item == 1) {
                    std::exit(0);
                }
            }
        } break;
    }
}

void Ui::open_shop_ui() {
    Ichiaji::program_state = Ichiaji::PS_UI_MENU;
    currently_open_menu    = MT_SHOP;
}

void Ui::open_game_over_ui() {
    auto callback = []([[maybe_unused]] uptr data) {
        currently_open_menu = MT_GAME_OVER;
    };

    Ichiaji::fullscreen_transition({1.0f, 1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, 2.0f, callback, 0);
    Ichiaji::program_state = Ichiaji::PS_UI_MENU;
}

void Ui::open_dialogue_ui(const StringID *strings, isize string_count) {
    Ichiaji::program_state        = Ichiaji::PS_UI_MENU;
    currently_open_menu           = MT_DIALOGUE;
    current_dialogue_strings      = strings;
    current_dialogue_string_count = string_count;
}
