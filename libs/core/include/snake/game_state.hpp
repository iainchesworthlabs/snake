// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string_view>

namespace snake
{

enum class GameState
{
    READY,
    RUNNING,
    PAUSED,
    GAME_OVER,
};

/// A lower-case name for the state, for logs and for the user interface to look up.
[[nodiscard]] std::string_view to_string_view(GameState state) noexcept;

} // namespace snake
