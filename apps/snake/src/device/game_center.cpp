// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/game_center.hpp"

namespace snake::ui::device
{

namespace
{

GameCenter* g_default_game_center = nullptr;

} // namespace

GameCenter* default_game_center() noexcept
{
    return g_default_game_center;
}

void set_default_game_center(GameCenter* game_center) noexcept
{
    g_default_game_center = game_center;
}

} // namespace snake::ui::device
