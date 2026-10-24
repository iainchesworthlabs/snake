// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace snake::input
{

/// What a key or gamepad button other than steering asks the game to do.
enum class Action : std::uint8_t
{
    PAUSE_RESUME,
    MENU,
    CONFIRM,
};

} // namespace snake::input
