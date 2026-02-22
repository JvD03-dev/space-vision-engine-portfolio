#pragma once

#include "engine/types.hpp"

namespace svl {

UiRect camera_button_rect(int viewport_width, int viewport_height);
bool rect_contains(const UiRect& rect, int x, int y);
void draw_camera_toggle_ui(int viewport_width, int viewport_height, bool tracking_enabled);

}  // namespace svl

