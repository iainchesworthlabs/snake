// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/game_state.hpp"

#include <utility>

namespace snake
{

std::string_view to_string_view(GameState state) noexcept
{
    switch (state)
    {
    case GameState::READY:
        return "ready";
    case GameState::RUNNING:
        return "running";
    case GameState::PAUSED:
        return "paused";
    case GameState::GAME_OVER:
        return "game over";
    }
    std::unreachable();
}

} // namespace snake
