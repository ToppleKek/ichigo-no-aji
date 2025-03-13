#pragma once

#include "strings.hpp"

namespace Ui {
void render_and_update_current_menu();
void open_shop_ui();
void open_dialogue_ui(const StringID *strings, isize string_count);
}
