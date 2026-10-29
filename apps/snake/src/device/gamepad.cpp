// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/gamepad.hpp"

#include <memory>
#include <utility>

namespace snake::ui::device
{

namespace
{

GamepadFactory g_factory;

} // namespace

void set_gamepad_factory(GamepadFactory factory)
{
    g_factory = std::move(factory);
}

std::unique_ptr<GamepadSource> gamepad_source()
{
    return g_factory ? g_factory() : make_gamepad_source();
}

} // namespace snake::ui::device
