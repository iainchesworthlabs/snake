// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace snake::ui
{

/// True when the device has a touch screen. The way of steering can be chosen only there: a computer is played with
/// the keyboard or a gamepad.
[[nodiscard]] bool has_touch_screen();

} // namespace snake::ui
