// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/gamepad.hpp"

#include <memory>

namespace snake::ui::device
{

/// The gamepads of a computer, read with SDL's gamepad subsystem (only that one: no window, no audio and no video of
/// SDL is used). Returns nothing when SDL cannot start it. The desktop operating systems' make_gamepad_source() is
/// this.
[[nodiscard]] std::unique_ptr<GamepadSource> make_sdl_gamepad_source();

} // namespace snake::ui::device
